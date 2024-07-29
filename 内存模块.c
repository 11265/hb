#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <pthread.h>

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

static MemoryRegion* find_cached_region(vm_address_t address) {
    pthread_mutex_lock(&regions_mutex);
    for (int i = 0; i < num_cached_regions; i++) {
        if (address >= cached_regions[i].base_address && 
            address < cached_regions[i].base_address + cached_regions[i].mapped_size) {
            cached_regions[i].access_count++;
            cached_regions[i].last_access = time(NULL);
            pthread_mutex_unlock(&regions_mutex);
            return &cached_regions[i];
        }
    }
    pthread_mutex_unlock(&regions_mutex);
    return NULL;
}

static void adjust_cache_size() {
    if (num_cached_regions == max_cached_regions) {
        if (max_cached_regions < MAX_CACHED_REGIONS) {
            max_cached_regions++;
            cached_regions = realloc(cached_regions, max_cached_regions * sizeof(MemoryRegion));
        }
    } else if (num_cached_regions < max_cached_regions / 2 && max_cached_regions > MIN_CACHED_REGIONS) {
        max_cached_regions--;
        cached_regions = realloc(cached_regions, max_cached_regions * sizeof(MemoryRegion));
    }
}

static int map_new_region(vm_address_t address) {
    kern_return_t kr;
    vm_address_t region_start = address & ~(REGION_SIZE - 1);
    vm_size_t size = REGION_SIZE;
    mach_vm_size_t bytes_read;

    pthread_mutex_lock(&regions_mutex);
    adjust_cache_size();
    
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

    vm_size_t aligned_size = ALIGN4(size);
    void *mapped_memory = mmap(NULL, aligned_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped_memory == MAP_FAILED) {
        pthread_mutex_unlock(&regions_mutex);
        return -1;
    }

    kr = vm_read_overwrite(target_task, region_start, size, (vm_address_t)mapped_memory, &bytes_read);
    if (kr != KERN_SUCCESS || bytes_read != size) {
        munmap(mapped_memory, aligned_size);
        pthread_mutex_unlock(&regions_mutex);
        return -1;
    }

    cached_regions[num_cached_regions].base_address = region_start;
    cached_regions[num_cached_regions].mapped_memory = mapped_memory;
    cached_regions[num_cached_regions].mapped_size = aligned_size;
    cached_regions[num_cached_regions].access_count = 1;
    cached_regions[num_cached_regions].last_access = time(NULL);
    num_cached_regions++;

    pthread_mutex_unlock(&regions_mutex);
    return 0;
}

static void* read_memory(vm_address_t address, size_t size) {
    MemoryRegion* region = find_cached_region(address);
    if (!region) {
        if (map_new_region(address) != 0) {
            return NULL;
        }
        region = find_cached_region(address);
        if (!region) {
            return NULL;
        }
    }
    return (void*)((char*)region->mapped_memory + (address - region->base_address));
}

static int write_memory(vm_address_t address, const void* value, size_t size) {
    MemoryRegion* region = find_cached_region(address);
    if (!region) {
        if (map_new_region(address) != 0) {
            return -1;
        }
        region = find_cached_region(address);
        if (!region) {
            return -1;
        }
    }
    memcpy((char*)region->mapped_memory + (address - region->base_address), value, size);
    return 0;
}

static size_t align_size(size_t size) {
    return ALIGN4(size);
}

static void* read_memory_aligned(vm_address_t address, size_t size) {
    size_t aligned_size = align_size(size);
    return read_memory(address, aligned_size);
}

static int write_memory_aligned(vm_address_t address, const void* value, size_t size) {
    size_t aligned_size = align_size(size);
    char aligned_buffer[aligned_size];
    memset(aligned_buffer, 0, aligned_size);
    memcpy(aligned_buffer, value, size);
    return write_memory(address, aligned_buffer, aligned_size);
}

static void* mapper_thread_func(void* arg) {
    ThreadInfo* info = (ThreadInfo*)arg;
    
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

        if (num_pending_requests > 0) {
            if (request.is_write) {
                int result = write_memory_aligned(request.address, request.write_value, sizeof(request.write_value));
                *(int*)request.result = result;
            } else {
                void* result = read_memory_aligned(request.address, sizeof(double));
                if (result) {
                    memcpy(request.result, result, sizeof(double));
                } else {
                    memset(request.result, 0, sizeof(double));
                }
            }

            pthread_mutex_lock(request.mutex);
            *request.completed = 1;
            pthread_cond_signal(request.cond);
            pthread_mutex_unlock(request.mutex);
        }
    }
    
    printf("Thread %d exiting\n", info->id);
    return NULL;
}

int initialize_memory_module(pid_t pid) {
    target_pid = pid;
    kern_return_t kr = task_for_pid(mach_task_self(), target_pid, &target_task);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "无法获取目标进程的任务端口\n");
        return -1;
    }

    cached_regions = malloc(max_cached_regions * sizeof(MemoryRegion));
    if (!cached_regions) {
        fprintf(stderr, "无法分配缓存区域内存\n");
        return -1;
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_pool[i].id = i;
        int result = pthread_create(&thread_pool[i].thread, NULL, mapper_thread_func, &thread_pool[i]);
        if (result != 0) {
            fprintf(stderr, "创建线程 %d 失败: %s\n", i, strerror(result));
            for (int j = 0; j < i; j++) {
                pthread_cancel(thread_pool[j].thread);
                pthread_join(thread_pool[j].thread, NULL);
            }
            free(cached_regions);
            return -1;
        }
    }

    return 0;
}

void cleanup_memory_module(void) {
    pthread_mutex_lock(&requests_mutex);
    stop_threads = 1;
    pthread_cond_broadcast(&request_cond);
    pthread_mutex_unlock(&requests_mutex);

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(thread_pool[i].thread, NULL);
    }

    for (int i = 0; i < num_cached_regions; i++) {
        munmap(cached_regions[i].mapped_memory, cached_regions[i].mapped_size);
    }
    free(cached_regions);

    mach_port_deallocate(mach_task_self(), target_task);
}

int32_t 读内存i32(vm_address_t address) {
    void* ptr = read_memory_aligned(address, sizeof(int32_t));
    return ptr ? *(int32_t*)ptr : 0;
}

int64_t 读内存i64(vm_address_t address) {
    void* ptr = read_memory_aligned(address, sizeof(int64_t));
    return ptr ? *(int64_t*)ptr : 0;
}

float 读内存f32(vm_address_t address) {
    void* ptr = read_memory_aligned(address, sizeof(float));
    return ptr ? *(float*)ptr : 0.0f;
}

double 读内存f64(vm_address_t address) {
    void* ptr = read_memory_aligned(address, sizeof(double));
    return ptr ? *(double*)ptr : 0.0;
}

int 写内存i32(vm_address_t address, int32_t value) {
    return write_memory_aligned(address, &value, sizeof(int32_t));
}

int 写内存i64(vm_address_t address, int64_t value) {
    return write_memory_aligned(address, &value, sizeof(int64_t));
}

int 写内存f32(vm_address_t address, float value) {
    return write_memory_aligned(address, &value, sizeof(float));
}

int 写内存f64(vm_address_t address, double value) {
    return write_memory_aligned(address, &value, sizeof(double));
}