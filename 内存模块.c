#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

static MemoryRegion *cached_regions = NULL;
static int num_cached_regions = 0;
static int max_cached_regions = INITIAL_CACHED_REGIONS;
static pid_t target_pid;
static task_t target_task;

static pthread_t mapper_thread;
static pthread_mutex_t regions_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t requests_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t request_cond = PTHREAD_COND_INITIALIZER;

static MemoryRequest pending_requests[MAX_PENDING_REQUESTS];
static int num_pending_requests = 0;

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

    void *mapped_memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped_memory == MAP_FAILED) {
        pthread_mutex_unlock(&regions_mutex);
        return -1;
    }

    kr = vm_read_overwrite(target_task, region_start, size, (vm_address_t)mapped_memory, &bytes_read);
    if (kr != KERN_SUCCESS || bytes_read != size) {
        munmap(mapped_memory, size);
        pthread_mutex_unlock(&regions_mutex);
        return -1;
    }

    cached_regions[num_cached_regions].base_address = region_start;
    cached_regions[num_cached_regions].mapped_memory = mapped_memory;
    cached_regions[num_cached_regions].mapped_size = size;
    cached_regions[num_cached_regions].access_count = 1;
    cached_regions[num_cached_regions].last_access = time(NULL);
    num_cached_regions++;

    pthread_mutex_unlock(&regions_mutex);
    return 0;
}

static void* read_memory(vm_address_t address, size_t size) {
    MemoryRegion* region = find_cached_region(address);
    if (region == NULL) {
        int result = map_new_region(address);
        if (result != 0) {
            fprintf(stderr, "无法映射内存区域，错误代码：%d\n", result);
            return NULL;
        }
        region = find_cached_region(address);
    }

    if (address < region->base_address || address >= region->base_address + region->mapped_size - size) {
        fprintf(stderr, "错误：地址 0x%llx 超出映射范围 (0x%llx - 0x%llx)\n", 
                (unsigned long long)address, 
                (unsigned long long)region->base_address, 
                (unsigned long long)(region->base_address + region->mapped_size));
        return NULL;
    }

    vm_address_t offset = address - region->base_address;
    return (void *)((char *)region->mapped_memory + offset);
}

int32_t 读内存i32(vm_address_t address) {
    void* ptr = read_memory(address, sizeof(int32_t));
    return ptr ? *(int32_t*)ptr : 0;
}

int64_t 读内存i64(vm_address_t address) {
    void* ptr = read_memory(address, sizeof(int64_t));
    return ptr ? *(int64_t*)ptr : 0;
}

float 读内存float(vm_address_t address) {
    void* ptr = read_memory(address, sizeof(float));
    return ptr ? *(float*)ptr : 0.0f;
}

double 读内存double(vm_address_t address) {
    void* ptr = read_memory(address, sizeof(double));
    return ptr ? *(double*)ptr : 0.0;
}

static void* async_read_memory(vm_address_t address, size_t size) {
    static char buffer[sizeof(double)];  // 足够大的缓冲区来存储任何类型
    memset(buffer, 0, sizeof(buffer));

    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    int completed = 0;

    pthread_mutex_lock(&requests_mutex);
    if (num_pending_requests >= MAX_PENDING_REQUESTS) {
        pthread_mutex_unlock(&requests_mutex);
        fprintf(stderr, "等待请求队列已满\n");
        return NULL;
    }

    pending_requests[num_pending_requests].address = address;
    pending_requests[num_pending_requests].result = buffer;
    pending_requests[num_pending_requests].cond = &cond;
    pending_requests[num_pending_requests].mutex = &mutex;
    pending_requests[num_pending_requests].completed = &completed;
    pending_requests[num_pending_requests].is_write = 0;
    num_pending_requests++;

    pthread_cond_signal(&request_cond);
    pthread_mutex_unlock(&requests_mutex);

    pthread_mutex_lock(&mutex);
    while (!completed) {
        pthread_cond_wait(&cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);

    return buffer;
}

int32_t 异步读内存i32(vm_address_t address) {
    void* ptr = async_read_memory(address, sizeof(int32_t));
    return ptr ? *(int32_t*)ptr : 0;
}

int64_t 异步读内存i64(vm_address_t address) {
    void* ptr = async_read_memory(address, sizeof(int64_t));
    return ptr ? *(int64_t*)ptr : 0;
}

float 异步读内存float(vm_address_t address) {
    void* ptr = async_read_memory(address, sizeof(float));
    return ptr ? *(float*)ptr : 0.0f;
}

double 异步读内存double(vm_address_t address) {
    void* ptr = async_read_memory(address, sizeof(double));
    return ptr ? *(double*)ptr : 0.0;
}

static int write_memory(vm_address_t address, const void* value, size_t size) {
    MemoryRegion* region = find_cached_region(address);
    if (region == NULL) {
        int result = map_new_region(address);
        if (result != 0) {
            fprintf(stderr, "无法映射内存区域，错误代码：%d\n", result);
            return -1;
        }
        region = find_cached_region(address);
    }

    if (address < region->base_address || address >= region->base_address + region->mapped_size - size) {
        fprintf(stderr, "错误：地址 0x%llx 超出映射范围 (0x%llx - 0x%llx)\n", 
                (unsigned long long)address, 
                (unsigned long long)region->base_address, 
                (unsigned long long)(region->base_address + region->mapped_size));
        return -1;
    }

    vm_address_t offset = address - region->base_address;
    memcpy((char *)region->mapped_memory + offset, value, size);

    kern_return_t kr = vm_write(target_task, address, (vm_offset_t)value, size);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "vm_write 失败: %s\n", mach_error_string(kr));
        return -1;
    }

    return 0;
}

int 写内存i32(vm_address_t address, int32_t value) {
    return write_memory(address, &value, sizeof(int32_t));
}

int 写内存i64(vm_address_t address, int64_t value) {
    return write_memory(address, &value, sizeof(int64_t));
}

int 写内存float(vm_address_t address, float value) {
    return write_memory(address, &value, sizeof(float));
}

int 写内存double(vm_address_t address, double value) {
    return write_memory(address, &value, sizeof(double));
}

static int async_write_memory(vm_address_t address, const void* value, size_t size) {
    int result = -1;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    int completed = 0;

    pthread_mutex_lock(&requests_mutex);
    if (num_pending_requests >= MAX_PENDING_REQUESTS) {
        pthread_mutex_unlock(&requests_mutex);
        fprintf(stderr, "等待请求队列已满\n");
        return -1;
    }

    pending_requests[num_pending_requests].address = address;
    pending_requests[num_pending_requests].result = &result;
    pending_requests[num_pending_requests].cond = &cond;
    pending_requests[num_pending_requests].mutex = &mutex;
    pending_requests[num_pending_requests].completed = &completed;
    pending_requests[num_pending_requests].is_write = 1;
    memcpy(&pending_requests[num_pending_requests].write_value, value, size);
    num_pending_requests++;

    pthread_cond_signal(&request_cond);
    pthread_mutex_unlock(&requests_mutex);

    pthread_mutex_lock(&mutex);
    while (!completed) {
        pthread_cond_wait(&cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);

    return result;
}

int 异步写内存i32(vm_address_t address, int32_t value) {
    return async_write_memory(address, &value, sizeof(int32_t));
}

int 异步写内存i64(vm_address_t address, int64_t value) {
    return async_write_memory(address, &value, sizeof(int64_t));
}

int 异步写内存float(vm_address_t address, float value) {
    return async_write_memory(address, &value, sizeof(float));
}

int 异步写内存double(vm_address_t address, double value) {
    return async_write_memory(address, &value, sizeof(double));
}

static void* mapper_thread_func(void* arg) {
    while (1) {
        pthread_mutex_lock(&requests_mutex);
        while (num_pending_requests == 0) {
            pthread_cond_wait(&request_cond, &requests_mutex);
        }

        MemoryRequest request = pending_requests[0];
        memmove(&pending_requests[0], &pending_requests[1], (num_pending_requests - 1) * sizeof(MemoryRequest));
        num_pending_requests--;
        pthread_mutex_unlock(&requests_mutex);

        if (request.is_write) {
            int result = write_memory(request.address, request.write_value, sizeof(request.write_value));
            *(int*)request.result = result;
        } else {
            void* result = read_memory(request.address, sizeof(double));
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
    return NULL;
}

int initialize_memory_module(pid_t pid) {
    kern_return_t kr;
    target_pid = pid;

    kr = task_for_pid(mach_task_self(), target_pid, &target_task);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "task_for_pid 失败: %s\n", mach_error_string(kr));
        return -1;
    }

    cached_regions = malloc(max_cached_regions * sizeof(MemoryRegion));
    if (cached_regions == NULL) {
        fprintf(stderr, "内存分配失败\n");
        return -1;
    }

    int result = pthread_create(&mapper_thread, NULL, mapper_thread_func, NULL);
    if (result != 0) {
        fprintf(stderr, "创建映射线程失败: %s\n", strerror(result));
        free(cached_regions);
        return -1;
    }

    return 0;
}

void cleanup_memory_module(void) {
    pthread_cancel(mapper_thread);
    pthread_join(mapper_thread, NULL);

    for (int i = 0; i < num_cached_regions; i++) {
        munmap(cached_regions[i].mapped_memory, cached_regions[i].mapped_size);
    }
    free(cached_regions);

    mach_port_deallocate(mach_task_self(), target_task);
}

int 搜索内存(vm_address_t start_address, vm_address_t end_address, const void* value, size_t size, vm_address_t* results, int max_results) {
    int count = 0;
    vm_address_t current_address = start_address;

    while (current_address < end_address && count < max_results) {
        void* memory = read_memory(current_address, size);
        if (memory && memcmp(memory, value, size) == 0) {
            results[count++] = current_address;
        }
        current_address += size;
    }

    return count;
}

int 异步搜索内存(vm_address_t start_address, vm_address_t end_address, const void* value, size_t size, vm_address_t* results, int max_results) {
    int count = 0;
    vm_address_t current_address = start_address;

    while (current_address < end_address && count < max_results) {
        void* memory = async_read_memory(current_address, size);
        if (memory && memcmp(memory, value, size) == 0) {
            results[count++] = current_address;
        }
        current_address += size;
    }

    return count;
}

int 获取内存区域信息(MemoryRegionInfo* regions, int max_regions) {
    vm_address_t address = 0;
    vm_size_t size;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object_name;
    int count = 0;

    while (count < max_regions) {
        kern_return_t kr = vm_region_64(target_task, &address, &size, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info, &info_count, &object_name);
        if (kr != KERN_SUCCESS) {
            break;
        }

        regions[count].start_address = address;
        regions[count].size = size;
        regions[count].protection = info.protection;
        count++;

        address += size;
    }

    return count;
}

int 设置内存保护(vm_address_t address, vm_size_t size, vm_prot_t new_protection) {
    kern_return_t kr = vm_protect(target_task, address, size, FALSE, new_protection);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "vm_protect 失败: %s\n", mach_error_string(kr));
        return -1;
    }
    return 0;
}

int 分配内存(vm_address_t* address, vm_size_t size) {
    kern_return_t kr = vm_allocate(target_task, address, size, VM_FLAGS_ANYWHERE);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "vm_allocate 失败: %s\n", mach_error_string(kr));
        return -1;
    }
    return 0;
}

int 释放内存(vm_address_t address, vm_size_t size) {
    kern_return_t kr = vm_deallocate(target_task, address, size);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "vm_deallocate 失败: %s\n", mach_error_string(kr));
        return -1;
    }
    return 0;
}

int 注入动态库(const char* dylib_path) {
    task_t remote_task = target_task;
    kern_return_t kr;

    // 获取 dlopen 的地址
    void* dlopen_address = dlsym(RTLD_DEFAULT, "dlopen");
    if (dlopen_address == NULL) {
        fprintf(stderr, "无法获取 dlopen 地址\n");
        return -1;
    }

    // 在远程进程中分配内存来存储动态库路径
    mach_vm_address_t remote_string_addr;
    kr = mach_vm_allocate(remote_task, &remote_string_addr, strlen(dylib_path) + 1, VM_FLAGS_ANYWHERE);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "无法在远程进程中分配内存: %s\n", mach_error_string(kr));
        return -1;
    }

    // 将动态库路径写入远程进程的内存
    kr = mach_vm_write(remote_task, remote_string_addr, (vm_offset_t)dylib_path, strlen(dylib_path) + 1);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "无法将动态库路径写入远程进程: %s\n", mach_error_string(kr));
        mach_vm_deallocate(remote_task, remote_string_addr, strlen(dylib_path) + 1);
        return -1;
    }

    // 创建远程线程来调用 dlopen
    thread_act_t remote_thread;
    kr = thread_create_running(remote_task, x86_THREAD_STATE64, (thread_state_t)&remote_thread, &remote_thread);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "无法创建远程线程: %s\n", mach_error_string(kr));
        mach_vm_deallocate(remote_task, remote_string_addr, strlen(dylib_path) + 1);
        return -1;
    }

    // 等待远程线程完成
    mach_msg_type_number_t thread_state_count = x86_THREAD_STATE64_COUNT;
    x86_thread_state64_t thread_state;
    kr = thread_get_state(remote_thread, x86_THREAD_STATE64, (thread_state_t)&thread_state, &thread_state_count);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "无法获取远程线程状态: %s\n", mach_error_string(kr));
        thread_terminate(remote_thread);
        mach_vm_deallocate(remote_task, remote_string_addr, strlen(dylib_path) + 1);
        return -1;
    }

    // 清理
    thread_terminate(remote_thread);
    mach_vm_deallocate(remote_task, remote_string_addr, strlen(dylib_path) + 1);

    return 0;
}