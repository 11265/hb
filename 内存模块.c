#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <pthread.h>

#define ALIGN4(size) (((size) + 3) & ~3)
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

static MemoryRegion* get_or_create_page(vm_address_t address) {
    vm_address_t page_address = address & PAGE_MASK;
    
    pthread_mutex_lock(&regions_mutex);
    
    // 查找现有的页面
    for (int i = 0; i < num_cached_regions; i++) {
        if (cached_regions[i].base_address == page_address) {
            cached_regions[i].access_count++;
            cached_regions[i].last_access = time(NULL);
            pthread_mutex_unlock(&regions_mutex);
            return &cached_regions[i];
        }
    }
    
    // 如果没有找到，创建新的页面
    if (num_cached_regions >= max_cached_regions) {
        // 如果缓存已满，移除最少使用的页面
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
    
    // 映射新页面
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

void* 读任意地址(vm_address_t address, size_t size) {
    MemoryRegion* region = get_or_create_page(address);
    if (!region) {
        return NULL;
    }
    
    size_t offset = address - region->base_address;
    if (offset + size > PAGE_SIZE) {
        // 如果读取跨越了页面边界，我们需要分配一个新的缓冲区
        void* buffer = malloc(size);
        if (!buffer) {
            return NULL;
        }
        
        size_t first_part = PAGE_SIZE - offset;
        memcpy(buffer, (char*)region->mapped_memory + offset, first_part);
        
        // 读取剩余部分
        size_t remaining = size - first_part;
        void* remaining_data = 读任意地址(address + first_part, remaining);
        if (!remaining_data) {
            free(buffer);
            return NULL;
        }
        
        memcpy((char*)buffer + first_part, remaining_data, remaining);
        free(remaining_data);
        
        return buffer;
    } else {
        // 如果读取没有跨越页面边界，直接返回映射内存中的指针
        return (char*)region->mapped_memory + offset;
    }
}

int 写任意地址(vm_address_t address, const void* data, size_t size) {
    MemoryRegion* region = get_or_create_page(address);
    if (!region) {
        return -1;
    }
    
    size_t offset = address - region->base_address;
    if (offset + size > PAGE_SIZE) {
        // 如果写入跨越了页面边界，我们需要分段写入
        size_t first_part = PAGE_SIZE - offset;
        memcpy((char*)region->mapped_memory + offset, data, first_part);
        
        // 写入剩余部分
        size_t remaining = size - first_part;
        return 写任意地址(address + first_part, (char*)data + first_part, remaining);
    } else {
        // 如果写入没有跨越页面边界，直接写入映射内存
        memcpy((char*)region->mapped_memory + offset, data, size);
        return 0;
    }
}

int32_t 读内存i32(vm_address_t address) {
    void* data = 读任意地址(address, sizeof(int32_t));
    if (!data) {
        return 0;
    }
    int32_t result = *(int32_t*)data;
    if (data != (void*)((char*)get_or_create_page(address)->mapped_memory + (address & (PAGE_SIZE - 1)))) {
        free(data);
    }
    return result;
}

int64_t 读内存i64(vm_address_t address) {
    void* data = 读任意地址(address, sizeof(int64_t));
    if (!data) {
        return 0;
    }
    int64_t result = *(int64_t*)data;
    if (data != (void*)((char*)get_or_create_page(address)->mapped_memory + (address & (PAGE_SIZE - 1)))) {
        free(data);
    }
    return result;
}

float 读内存f32(vm_address_t address) {
    void* data = 读任意地址(address, sizeof(float));
    if (!data) {
        return 0.0f;
    }
    float result = *(float*)data;
    if (data != (void*)((char*)get_or_create_page(address)->mapped_memory + (address & (PAGE_SIZE - 1)))) {
        free(data);
    }
    return result;
}

double 读内存f64(vm_address_t address) {
    void* data = 读任意地址(address, sizeof(double));
    if (!data) {
        return 0.0;
    }
    double result = *(double*)data;
    if (data != (void*)((char*)get_or_create_page(address)->mapped_memory + (address & (PAGE_SIZE - 1)))) {
        free(data);
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
    while (1) {
        pthread_mutex_lock(&requests_mutex);
        while (num_pending_requests == 0 && !stop_threads) {
            pthread_cond_wait(&request_cond, &requests_mutex);
        }
        
        if (stop_threads && num_pending_requests == 0) {
            pthread_mutex_unlock(&requests_mutex);
            break;
        }

        MemoryRequest request;
        if (num_pending_requests > 0) {
            request = pending_requests[0];
            memmove(&pending_requests[0], &pending_requests[1], (num_pending_requests - 1) * sizeof(MemoryRequest));
            num_pending_requests--;
        }
        pthread_mutex_unlock(&requests_mutex);

        if (request.operation == 1) { // 写操作
            int result = 写任意地址(request.address, request.buffer, request.size);
            *(int*)request.result = result;
        } else { // 读操作
            void* result = 读任意地址(request.address, request.size);
            if (result) {
                memcpy(request.result, result, request.size);
                if (result != (void*)((char*)get_or_create_page(request.address)->mapped_memory + (request.address & (PAGE_SIZE - 1)))) {
                    free(result);
                }
            } else {
                memset(request.result, 0, request.size);
            }
        }
    }
    
    return NULL;
}

static void* mapper_thread_func(void* arg) {
    return 处理内存请求(arg);
}

int 初始化内存模块(pid_t pid) {
    target_pid = pid;
    kern_return_t kr = task_for_pid(mach_task_self(), target_pid, &target_task);
    if (kr != KERN_SUCCESS) {
        return -1;
    }
    
    cached_regions = malloc(max_cached_regions * sizeof(MemoryRegion));
    if (!cached_regions) {
        return -1;
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_pool[i].id = i;
        pthread_create(&thread_pool[i].thread, NULL, mapper_thread_func, &thread_pool[i]);
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
    cached_regions = NULL;
    num_cached_regions = 0;
    
    mach_port_deallocate(mach_task_self(), target_task);
}