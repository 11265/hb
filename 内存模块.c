#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <pthread.h>

#define ALIGN4(size) (((size) + 3) & ~3)
#define LARGE_MAPPING_THRESHOLD (1024 * 1024)  // 1MB threshold for large mappings

#define MEMORY_POOL_SIZE (1024 * 1024)  // 1MB memory pool
#define MAX_SMALL_ALLOCATION 256  // Maximum size for small allocations

typedef struct MemoryBlock {
    size_t size;
    struct MemoryBlock* next;
} MemoryBlock;

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

// Memory pool variables
static char* memory_pool = NULL;
static MemoryBlock* free_list = NULL;
static pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;

void init_memory_pool() {
    memory_pool = (char*)malloc(MEMORY_POOL_SIZE);
    if (memory_pool == NULL) {
        fprintf(stderr, "Failed to allocate memory pool\n");
        exit(1);
    }
    
    free_list = (MemoryBlock*)memory_pool;
    free_list->size = MEMORY_POOL_SIZE - sizeof(MemoryBlock);
    free_list->next = NULL;
}

void* 内存分配(size_t size) {
    if (size > MAX_SMALL_ALLOCATION) {
        return malloc(size);
    }

    size = ALIGN4(size + sizeof(MemoryBlock));
    
    pthread_mutex_lock(&pool_mutex);
    
    MemoryBlock* prev = NULL;
    MemoryBlock* current = free_list;
    
    while (current != NULL) {
        if (current->size >= size) {
            if (current->size > size + sizeof(MemoryBlock)) {
                MemoryBlock* new_block = (MemoryBlock*)((char*)current + size);
                new_block->size = current->size - size;
                new_block->next = current->next;
                current->size = size;
                
                if (prev == NULL) {
                    free_list = new_block;
                } else {
                    prev->next = new_block;
                }
            } else {
                if (prev == NULL) {
                    free_list = current->next;
                } else {
                    prev->next = current->next;
                }
            }
            
            pthread_mutex_unlock(&pool_mutex);
            return (char*)current + sizeof(MemoryBlock);
        }
        
        prev = current;
        current = current->next;
    }
    
    pthread_mutex_unlock(&pool_mutex);
    return malloc(size);  // 如果内存池中没有足够的空间,使用系统的malloc
}

void 内存释放(void* ptr) {
    if (ptr == NULL) {
        return;
    }

    if (ptr < (void*)memory_pool || ptr >= (void*)(memory_pool + MEMORY_POOL_SIZE)) {
        free(ptr);  // 如果不是来自内存池的内存,使用系统的free
        return;
    }
    
    MemoryBlock* block = (MemoryBlock*)((char*)ptr - sizeof(MemoryBlock));
    
    pthread_mutex_lock(&pool_mutex);
    
    block->next = free_list;
    free_list = block;
    
    // Coalesce adjacent free blocks
    MemoryBlock* current = free_list;
    while (current != NULL && current->next != NULL) {
        if ((char*)current + current->size == (char*)current->next) {
            current->size += current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
    
    pthread_mutex_unlock(&pool_mutex);
}

MemoryRegion* get_or_create_mapping(vm_address_t address, size_t size) {
    pthread_mutex_lock(&regions_mutex);
    
    // Check if we already have a mapping that covers this address range
    for (int i = 0; i < num_cached_regions; i++) {
        if (cached_regions[i].base_address <= address &&
            address + size <= cached_regions[i].base_address + cached_regions[i].mapped_size) {
            cached_regions[i].access_count++;
            cached_regions[i].last_access = time(NULL);
            pthread_mutex_unlock(&regions_mutex);
            return &cached_regions[i];
        }
    }
    
    // If not found, create a new mapping
    size_t mapping_size = (size > LARGE_MAPPING_THRESHOLD) ? size : PAGE_SIZE;
    vm_address_t mapping_start = address & PAGE_MASK;
    
    void* mapped_memory = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped_memory == MAP_FAILED) {
        pthread_mutex_unlock(&regions_mutex);
        return NULL;
    }
    
    mach_vm_size_t bytes_read;
    kern_return_t kr = vm_read_overwrite(target_task, mapping_start, mapping_size, (vm_address_t)mapped_memory, &bytes_read);
    if (kr != KERN_SUCCESS || bytes_read != mapping_size) {
        munmap(mapped_memory, mapping_size);
        pthread_mutex_unlock(&regions_mutex);
        return NULL;
    }
    
    // Add the new mapping to our cache
    if (num_cached_regions >= max_cached_regions) {
        // If cache is full, remove the least recently used mapping
        int lru_index = 0;
        time_t oldest_access = time(NULL);
        for (int i = 0; i < num_cached_regions; i++) {
            if (cached_regions[i].last_access < oldest_access) {
                lru_index = i;
                oldest_access = cached_regions[i].last_access;
            }
        }
        munmap(cached_regions[lru_index].mapped_memory, cached_regions[lru_index].mapped_size);
        memmove(&cached_regions[lru_index], &cached_regions[lru_index + 1], 
                (num_cached_regions - lru_index - 1) * sizeof(MemoryRegion));
        num_cached_regions--;
    }
    
    cached_regions[num_cached_regions].base_address = mapping_start;
    cached_regions[num_cached_regions].mapped_memory = mapped_memory;
    cached_regions[num_cached_regions].mapped_size = mapping_size;
    cached_regions[num_cached_regions].access_count = 1;
    cached_regions[num_cached_regions].last_access = time(NULL);
    cached_regions[num_cached_regions].is_large_mapping = (mapping_size > PAGE_SIZE);
    
    num_cached_regions++;
    
    pthread_mutex_unlock(&regions_mutex);
    return &cached_regions[num_cached_regions - 1];
}

void* 读任意地址(vm_address_t address, void* buffer, size_t size) {
    MemoryRegion* region = get_or_create_mapping(address, size);
    if (!region) {
        return NULL;
    }
    
    size_t offset = address - region->base_address;
    memcpy(buffer, (char*)region->mapped_memory + offset, size);
    
    return buffer;
}

// 继续上一部分的代码...

int 写任意地址(vm_address_t address, const void* data, size_t size) {
    MemoryRegion* region = get_or_create_mapping(address, size);
    if (!region) {
        return -1;
    }
    
    size_t offset = address - region->base_address;
    memcpy((char*)region->mapped_memory + offset, data, size);
    
    // For large mappings, we need to write back to the target process
    if (region->is_large_mapping) {
        kern_return_t kr = vm_write(target_task, address, (vm_offset_t)data, size);
        if (kr != KERN_SUCCESS) {
            return -1;
        }
    }
    
    return 0;
}

int32_t 读内存i32(vm_address_t address) {
    int32_t value;
    if (读任意地址(address, &value, sizeof(int32_t)) == NULL) {
        return 0;
    }
    return value;
}

int64_t 读内存i64(vm_address_t address) {
    int64_t value;
    if (读任意地址(address, &value, sizeof(int64_t)) == NULL) {
        return 0;
    }
    return value;
}

float 读内存f32(vm_address_t address) {
    float value;
    if (读任意地址(address, &value, sizeof(float)) == NULL) {
        return 0.0f;
    }
    return value;
}

double 读内存f64(vm_address_t address) {
    double value;
    if (读任意地址(address, &value, sizeof(double)) == NULL) {
        return 0.0;
    }
    return value;
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

void* worker_thread(void* arg) {
    ThreadInfo* info = (ThreadInfo*)arg;
    
    while (!stop_threads) {
        MemoryRequest request;
        int has_request = 0;
        
        pthread_mutex_lock(&requests_mutex);
        while (num_pending_requests == 0 && !stop_threads) {
            pthread_cond_wait(&request_cond, &requests_mutex);
        }
        
        if (num_pending_requests > 0) {
            request = pending_requests[--num_pending_requests];
            has_request = 1;
        }
        pthread_mutex_unlock(&requests_mutex);
        
        if (has_request) {
            switch (request.operation) {
                case 0: // Read
                    request.result = 读任意地址(request.address, request.size);
                    break;
                case 1: // Write
                    request.result = (void*)(intptr_t)写任意地址(request.address, request.buffer, request.size);
                    break;
            }
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
    
    cached_regions = (MemoryRegion*)malloc(max_cached_regions * sizeof(MemoryRegion));
    if (!cached_regions) {
        return -1;
    }
    
    init_memory_pool();
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_pool[i].id = i;
        pthread_create(&thread_pool[i].thread, NULL, worker_thread, &thread_pool[i]);
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
    
    free(memory_pool);
    
    mach_port_deallocate(mach_task_self(), target_task);
}

// Helper function to add a request to the queue
static int add_request(int operation, vm_address_t address, void* buffer, size_t size) {
    pthread_mutex_lock(&requests_mutex);
    
    if (num_pending_requests >= MAX_PENDING_REQUESTS) {
        pthread_mutex_unlock(&requests_mutex);
        return -1;
    }
    
    MemoryRequest* request = &pending_requests[num_pending_requests++];
    request->operation = operation;
    request->address = address;
    request->buffer = buffer;
    request->size = size;
    request->result = NULL;
    
    pthread_cond_signal(&request_cond);
    pthread_mutex_unlock(&requests_mutex);
    
    return 0;
}

// These functions can be used to queue read/write operations for the worker threads
void* 异步读任意地址(vm_address_t address, size_t size) {
    if (add_request(0, address, NULL, size) != 0) {
        return NULL;
    }
    // In a real implementation, you'd wait for the result here
    return NULL;
}

int 异步写任意地址(vm_address_t address, const void* data, size_t size) {
    return add_request(1, address, (void*)data, size);
}