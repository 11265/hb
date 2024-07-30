// 内存模块.c

#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <errno.h>
#include <mach/mach_error.h>

#define PAGE_SIZE 4096
#define PAGE_MASK (~(PAGE_SIZE - 1))

typedef struct {
    pthread_t thread;
    int id;
} ThreadInfo;

static MemoryRegion *cached_regions = NULL;
static int num_cached_regions = 0;
static int max_cached_regions = INITIAL_CACHED_REGIONS;
static pid_t target_pid;
static task_t target_task;

static ThreadInfo thread_pool[NUM_THREADS];
static pthread_mutex_t regions_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t requests_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t request_cond = PTHREAD_COND_INITIALIZER;

static MemoryRequest pending_requests[MAX_PENDING_REQUESTS];
static int num_pending_requests = 0;
static int stop_threads = 0;

static MemoryPool memory_pool;

void 初始化内存池() {
    memory_pool.free_blocks = NULL;
    pthread_mutex_init(&memory_pool.mutex, NULL);

    for (int i = 0; i < MEMORY_POOL_BLOCK_COUNT; i++) {
        MemoryBlock* block = (MemoryBlock*)malloc(sizeof(MemoryBlock));
        if (block == NULL) {
            fprintf(stderr, "Failed to allocate memory block\n");
            continue;
        }
        block->memory = malloc(MEMORY_POOL_BLOCK_SIZE);
        if (block->memory == NULL) {
            fprintf(stderr, "Failed to allocate memory for block\n");
            free(block);
            continue;
        }
        block->next = memory_pool.free_blocks;
        memory_pool.free_blocks = block;
    }
}

void* 内存池分配(size_t size) {
    if (size > MEMORY_POOL_BLOCK_SIZE) {
        return malloc(size);
    }

    pthread_mutex_lock(&memory_pool.mutex);
    if (memory_pool.free_blocks == NULL) {
        pthread_mutex_unlock(&memory_pool.mutex);
        return malloc(size);
    }

    MemoryBlock* block = memory_pool.free_blocks;
    memory_pool.free_blocks = block->next;
    pthread_mutex_unlock(&memory_pool.mutex);

    return block->memory;
}

void 内存池释放(void* ptr) {
    if (ptr == NULL) {
        return;
    }

    pthread_mutex_lock(&memory_pool.mutex);
    MemoryBlock* block = (MemoryBlock*)((char*)ptr - sizeof(MemoryBlock));
    block->next = memory_pool.free_blocks;
    memory_pool.free_blocks = block;
    pthread_mutex_unlock(&memory_pool.mutex);
}

void 清理内存池() {
    MemoryBlock* current = memory_pool.free_blocks;
    while (current != NULL) {
        MemoryBlock* next = current->next;
        free(current->memory);
        free(current);
        current = next;
    }
    pthread_mutex_destroy(&memory_pool.mutex);
}

MemoryRegion* get_or_create_page(vm_address_t address) {
    if (address == 0) {
        fprintf(stderr, "Invalid address: 0\n");
        return NULL;
    }

    vm_address_t page_address = address & PAGE_MASK;
    
    pthread_mutex_lock(&regions_mutex);
    
    for (int i = 0; i < num_cached_regions; i++) {
        if (cached_regions[i].base_address == page_address) {
            cached_regions[i].access_count++;
            cached_regions[i].last_access = time(NULL);
            pthread_mutex_unlock(&regions_mutex);
            return &cached_regions[i];
        }
    }
    
    if (num_cached_regions >= max_cached_regions) {
        int least_used_index = 0;
        uint32_t least_used_count = UINT32_MAX;
        time_t oldest_access = time(NULL);
        
        for (int i = 0; i < num_cached_regions; i++) {
            if (cached_regions[i].access_count < least_used_count ||
                (cached_regions[i].access_count == least_used_count && 
                 cached_regions[i].last_access < oldest_access)) {
                least_used_index = i;
                least_used_count = cached_regions[i].access_count;
                oldest_access = cached_regions[i].last_access;
            }
        }
        
        if (munmap(cached_regions[least_used_index].mapped_memory, cached_regions[least_used_index].mapped_size) != 0) {
            fprintf(stderr, "Failed to unmap memory: %s\n", strerror(errno));
        }
        memmove(&cached_regions[least_used_index], &cached_regions[least_used_index + 1], 
                (num_cached_regions - least_used_index - 1) * sizeof(MemoryRegion));
        num_cached_regions--;
    }
    
    void* mapped_memory = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped_memory == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        pthread_mutex_unlock(&regions_mutex);
        return NULL;
    }
    
    mach_vm_size_t bytes_read;
    kern_return_t kr = vm_read_overwrite(target_task, page_address, PAGE_SIZE, (vm_address_t)mapped_memory, &bytes_read);
    if (kr != KERN_SUCCESS || bytes_read != PAGE_SIZE) {
        fprintf(stderr, "vm_read_overwrite failed: %s\n", mach_error_string(kr));
        munmap(mapped_memory, PAGE_SIZE);
        pthread_mutex_unlock(&regions_mutex);
        return NULL;
    }
    
    cached_regions[num_cached_regions].base_address = page_address;
    cached_regions[num_cached_regions].mapped_memory = mapped_memory;
    cached_regions[num_cached_regions].mapped_size = PAGE_SIZE;
    cached_regions[num_cached_regions].access_count = 1;
    cached_regions[num_cached_regions].last_access = time(NULL);
    
    num_cached_regions++;
    
    pthread_mutex_unlock(&regions_mutex);
    return &cached_regions[num_cached_regions - 1];
}

void* 读任意地址(vm_address_t address, void* buffer, size_t size) {
    if (address == 0 || buffer == NULL || size == 0) {
        fprintf(stderr, "Invalid parameters in 读任意地址\n");
        return NULL;
    }

    MemoryRegion* region = get_or_create_page(address);
    if (!region) {
        fprintf(stderr, "Failed to get or create page for address 0x%llx\n", address);
        return NULL;
    }
    
    size_t offset = address - region->base_address;
    size_t available = region->mapped_size - offset;
    size_t to_copy = (size <= available) ? size : available;
    
    memcpy(buffer, (char*)region->mapped_memory + offset, to_copy);
    
    if (to_copy < size) {
        void* remaining_buffer = (char*)buffer + to_copy;
        size_t remaining_size = size - to_copy;
        return 读任意地址(address + to_copy, remaining_buffer, remaining_size);
    }
    
    return buffer;
}

int 写任意地址(vm_address_t address, const void* data, size_t size) {
    if (address == 0 || data == NULL || size == 0) {
        fprintf(stderr, "Invalid parameters in 写任意地址\n");
        return -1;
    }

    kern_return_t kr = vm_write(target_task, address, (vm_offset_t)data, size);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "vm_write failed: %s\n", mach_error_string(kr));
        return -1;
    }
    return 0;
}

int32_t 读内存i32(vm_address_t address) {
    int32_t value = 0;
    if (读任意地址(address, &value, sizeof(value)) == NULL) {
        fprintf(stderr, "Failed to read i32 at address 0x%llx\n", address);
        return 0;
    }
    return value;
}

int64_t 读内存i64(vm_address_t address) {
    int64_t value = 0;
    if (读任意地址(address, &value, sizeof(value)) == NULL) {
        fprintf(stderr, "Failed to read i64 at address 0x%llx\n", address);
        return 0;
    }
    return value;
}

float 读内存f32(vm_address_t address) {
    float value = 0.0f;
    if (读任意地址(address, &value, sizeof(value)) == NULL) {
        fprintf(stderr, "Failed to read f32 at address 0x%llx\n", address);
        return 0.0f;
    }
    return value;
}

double 读内存f64(vm_address_t address) {
    double value = 0.0;
    if (读任意地址(address, &value, sizeof(value)) == NULL) {
        fprintf(stderr, "Failed to read f64 at address 0x%llx\n", address);
        return 0.0;
    }
    return value;
}

int 写内存i32(vm_address_t address, int32_t value) {
    return 写任意地址(address, &value, sizeof(value));
}

int 写内存i64(vm_address_t address, int64_t value) {
    return 写任意地址(address, &value, sizeof(value));
}

int 写内存f32(vm_address_t address, float value) {
    return 写任意地址(address, &value, sizeof(value));
}

int 写内存f64(vm_address_t address, double value) {
    return 写任意地址(address, &value, sizeof(value));
}

void* 处理内存请求(void* arg) {
    ThreadInfo* info = (ThreadInfo*)arg;
    
    while (!stop_threads) {
        MemoryRequest request;
        int have_request = 0;
        
        pthread_mutex_lock(&requests_mutex);
        while (num_pending_requests == 0 && !stop_threads) {
            pthread_cond_wait(&request_cond, &requests_mutex);
        }
        
        if (num_pending_requests > 0) {
            request = pending_requests[--num_pending_requests];
            have_request = 1;
        }
        pthread_mutex_unlock(&requests_mutex);
        
        if (have_request) {
            if (request.operation == 0) {  // 读操作
                request.result = 读任意地址(request.address, request.buffer, request.size);
                if (request.result == NULL) {
                    fprintf(stderr, "Thread %d: 读取地址 0x%llx 失败\n", info->id, request.address);
                }
            } else {  // 写操作
                int write_result = 写任意地址(request.address, request.buffer, request.size);
                request.result = (void*)(intptr_t)write_result;
                if (write_result != 0) {
                    fprintf(stderr, "Thread %d: 写入地址 0x%llx 失败\n", info->id, request.address);
                }
            }
        }
    }
    
    return NULL;
}

int 初始化内存模块(pid_t pid) {
    target_pid = pid;
    kern_return_t kr = task_for_pid(mach_task_self(), target_pid, &target_task);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "Failed to get task for pid %d: %s\n", pid, mach_error_string(kr));
        return -1;
    }
    
    cached_regions = (MemoryRegion*)malloc(sizeof(MemoryRegion) * max_cached_regions);
    if (cached_regions == NULL) {
        fprintf(stderr, "Failed to allocate memory for cached regions\n");
        return -1;
    }
    
    初始化内存池();
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_pool[i].id = i;
        if (pthread_create(&thread_pool[i].thread, NULL, 处理内存请求, &thread_pool[i]) != 0) {
            fprintf(stderr, "Failed to create thread %d\n", i);
            return -1;
        }
    }
    
    return 0;
}

void 关闭内存模块() {
    stop_threads = 1;
    pthread_cond_broadcast(&request_cond);
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(thread_pool[i].thread, NULL);
    }
    
    for (int i = 0; i < num_cached_regions; i++) {
        if (munmap(cached_regions[i].mapped_memory, cached_regions[i].mapped_size) != 0) {
            fprintf(stderr, "Failed to unmap memory for region %d: %s\n", i, strerror(errno));
        }
    }
    
    free(cached_regions);
    清理内存池();
    
    mach_port_deallocate(mach_task_self(), target_task);
}