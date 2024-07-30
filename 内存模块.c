// 内存模块.c

#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

#define ALIGN4(size) (((size) + 3) & ~3)

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
        block->memory = malloc(MEMORY_POOL_BLOCK_SIZE);
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
    if ((uintptr_t)ptr % MEMORY_POOL_BLOCK_SIZE == 0) {
        MemoryBlock* block = (MemoryBlock*)((char*)ptr - sizeof(MemoryBlock));
        block->next = memory_pool.free_blocks;
        memory_pool.free_blocks = block;
    } else {
        free(ptr);  // 如果不是来自我们的池，使用常规的 free
    }
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
        
        munmap(cached_regions[least_used_index].mapped_memory, cached_regions[least_used_index].mapped_size);
        memmove(&cached_regions[least_used_index], &cached_regions[least_used_index + 1], 
                (num_cached_regions - least_used_index - 1) * sizeof(MemoryRegion));
        num_cached_regions--;
    }
    
    void* mapped_memory = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped_memory == MAP_FAILED) {
        pthread_mutex_unlock(&regions_mutex);
        return NULL;
    }
    
    mach_vm_size_t bytes_read;
    kern_return_t kr = vm_read_overwrite(target_task, page_address, PAGE_SIZE, (vm_address_t)mapped_memory, &bytes_read);
    if (kr != KERN_SUCCESS || bytes_read != PAGE_SIZE) {
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
    MemoryRegion* region = get_or_create_page(address);
    if (!region) {
        return NULL;
    }
    
    size_t offset = address - region->base_address;
    
    if (offset + size > PAGE_SIZE) {
        size_t first_part = PAGE_SIZE - offset;
        memcpy(buffer, (char*)region->mapped_memory + offset, first_part);
        
        size_t remaining = size - first_part;
        void* remaining_data = 读任意地址(address + first_part, (char*)buffer + first_part, remaining);
        if (!remaining_data) {
            return NULL;
        }
    } else {
        memcpy(buffer, (char*)region->mapped_memory + offset, size);
    }
    
    return buffer;
}

int 写任意地址(vm_address_t address, const void* data, size_t size) {
    MemoryRegion* region = get_or_create_page(address);
    if (!region) {
        return -1;
    }
    
    size_t offset = address - region->base_address;
    if (offset + size > PAGE_SIZE) {
        size_t first_part = PAGE_SIZE - offset;
        memcpy((char*)region->mapped_memory + offset, data, first_part);
        
        size_t remaining = size - first_part;
        return 写任意地址(address + first_part, (char*)data + first_part, remaining);
    } else {
        memcpy((char*)region->mapped_memory + offset, data, size);
        return 0;
    }
}

int32_t 读内存i32(vm_address_t address) {
    int32_t result;
    if (读任意地址(address, &result, sizeof(int32_t)) == NULL) {
        return 0;
    }
    return result;
}

int64_t 读内存i64(vm_address_t address) {
    int64_t result;
    if (读任意地址(address, &result, sizeof(int64_t)) == NULL) {
        return 0;
    }
    return result;
}

float 读内存f32(vm_address_t address) {
    float result;
    if (读任意地址(address, &result, sizeof(float)) == NULL) {
        return 0.0f;
    }
    return result;
}

double 读内存f64(vm_address_t address) {
    double result;
    if (读任意地址(address, &result, sizeof(double)) == NULL) {
        return 0.0;
    }
    return result;
}

int 写内存i32(vm_address_t address, int32_t value) {
    return 写任意地址(address, &value, sizeof(int32_t));
}

int 写内存i64(vm_address_t address, int64_t value) {
    return 写任意地址(address, &value, sizeof(int64_t));
}

int 写内存f32(vm_address_t address, float value) {
    return 写任意地址(address, &value, sizeof(float));
}

int 写内存f64(vm_address_t address, double value) {
    return 写任意地址(address, &value, sizeof(double));
}

void* 处理内存请求(void* arg) {
    ThreadInfo* info = (ThreadInfo*)arg;
    
    while (!stop_threads) {
        pthread_mutex_lock(&requests_mutex);
        while (num_pending_requests == 0 && !stop_threads) {
            pthread_cond_wait(&request_cond, &requests_mutex);
        }
        
        if (stop_threads) {
            pthread_mutex_unlock(&requests_mutex);
            break;
        }
        
        MemoryRequest request = pending_requests[--num_pending_requests];
        pthread_mutex_unlock(&requests_mutex);
        
        if (request.operation == 0) { // Read operation
            request.result = 读任意地址(request.address, request.buffer, request.size);
        } else { // Write operation
            request.result = (void*)(intptr_t)写任意地址(request.address, request.buffer, request.size);
        }
    }
    
    return NULL;
}

int 初始化内存模块(pid_t pid) {
    target_pid = pid;
    kern_return_t kr = task_for_pid(mach_task_self(), target_pid, &target_task);
    if (kr != KERN_SUCCESS) {
        return -1;
    }
    
    cached_regions = (MemoryRegion*)malloc(sizeof(MemoryRegion) * max_cached_regions);
    if (!cached_regions) {
        return -1;
    }
    
    初始化内存池();
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_pool[i].id = i;
        pthread_create(&thread_pool[i].thread, NULL, 处理内存请求, &thread_pool[i]);
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
        munmap(cached_regions[i].mapped_memory, cached_regions[i].mapped_size);
    }
    
    free(cached_regions);
    清理内存池();
}