#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <mach/arm/thread_status.h>

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
    vm_size_t bytes_read;

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
            fprintf(stderr, "无法映射内存区域,错误代码:%d\n", result);
            return NULL;
        }
        region = find_cached_region(address);
    }

    if (address < region->base_address || address >= region->base_address + region->mapped_size - size) {
        fprintf(stderr, "错误:地址 0x%llx 超出映射范围 (0x%llx - 0x%llx)\n", 
                (unsigned long long)address, 
                (unsigned long long)region->base_address, 
                (unsigned long long)(region->base_address + region->mapped_size));
        return NULL;
    }

    return (void*)((char*)region->mapped_memory + (address - region->base_address));
}

int initialize_memory_module(pid_t pid) {
    kern_return_t kr;
    target_pid = pid;
    kr = task_for_pid(mach_task_self(), target_pid, &target_task);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "无法获取目标进程的任务端口,错误代码:%d\n", kr);
        return -1;
    }

    cached_regions = malloc(max_cached_regions * sizeof(MemoryRegion));
    if (cached_regions == NULL) {
        fprintf(stderr, "无法分配内存\n");
        return -1;
    }

    // 初始化其他必要的资源...

    return 0;
}

void cleanup_memory_module() {
    for (int i = 0; i < num_cached_regions; i++) {
        munmap(cached_regions[i].mapped_memory, cached_regions[i].mapped_size);
    }
    free(cached_regions);
    // 清理其他资源...
}

int32_t 异步读内存i32(vm_address_t address) {
    void* data = read_memory(address, sizeof(int32_t));
    if (data == NULL) {
        return 0;
    }
    return *(int32_t*)data;
}

int64_t 异步读内存i64(vm_address_t address) {
    void* data = read_memory(address, sizeof(int64_t));
    if (data == NULL) {
        return 0;
    }
    return *(int64_t*)data;
}

float 异步读内存float(vm_address_t address) {
    void* data = read_memory(address, sizeof(float));
    if (data == NULL) {
        return 0.0f;
    }
    return *(float*)data;
}

double 异步读内存double(vm_address_t address) {
    void* data = read_memory(address, sizeof(double));
    if (data == NULL) {
        return 0.0;
    }
    return *(double*)data;
}

int 异步写内存i32(vm_address_t address, int32_t value) {
    kern_return_t kr;
    kr = vm_write(target_task, address, (vm_offset_t)&value, sizeof(int32_t));
    return (kr == KERN_SUCCESS) ? 0 : -1;
}

int 异步写内存i64(vm_address_t address, int64_t value) {
    kern_return_t kr;
    kr = vm_write(target_task, address, (vm_offset_t)&value, sizeof(int64_t));
    return (kr == KERN_SUCCESS) ? 0 : -1;
}

int 异步写内存float(vm_address_t address, float value) {
    kern_return_t kr;
    kr = vm_write(target_task, address, (vm_offset_t)&value, sizeof(float));
    return (kr == KERN_SUCCESS) ? 0 : -1;
}

int 异步写内存double(vm_address_t address, double value) {
    kern_return_t kr;
    kr = vm_write(target_task, address, (vm_offset_t)&value, sizeof(double));
    return (kr == KERN_SUCCESS) ? 0 : -1;
}

int 获取内存区域信息(MemoryRegion* regions, int max_regions) {
    vm_address_t address = 0;
    vm_size_t size;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t info_count;
    vm_region_flavor_t flavor = VM_REGION_BASIC_INFO_64;
    mach_port_t object_name;
    int count = 0;

    while (count < max_regions) {
        info_count = VM_REGION_BASIC_INFO_COUNT_64;
        kern_return_t kr = vm_region_64(target_task, &address, &size, flavor,
                                        (vm_region_info_t)&info, &info_count, &object_name);
        if (kr != KERN_SUCCESS) {
            break;
        }

        regions[count].base_address = address;
        regions[count].mapped_size = size;
        regions[count].access_count = 0;
        regions[count].last_access = 0;
        count++;

        address += size;
    }

    return count;
}

int 注入动态库(const char* dylib_path) {
    kern_return_t kr;
    mach_vm_address_t remote_string_addr = 0;
    mach_vm_size_t remote_string_size = strlen(dylib_path) + 1;

    // 在远程进程中分配内存
    kr = mach_vm_allocate(target_task, &remote_string_addr, remote_string_size, VM_FLAGS_ANYWHERE);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "无法在远程进程中分配内存\n");
        return -1;
    }

    // 将动态库路径写入远程进程内存
    kr = mach_vm_write(target_task, remote_string_addr, (vm_offset_t)dylib_path, remote_string_size);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "无法写入远程进程内存\n");
        mach_vm_deallocate(target_task, remote_string_addr, remote_string_size);
        return -1;
    }

    // 获取 dlopen 函数的地址
    void* dlopen_addr = dlsym(RTLD_DEFAULT, "dlopen");
    if (dlopen_addr == NULL) {
        fprintf(stderr, "无法获取 dlopen 函数地址\n");
        mach_vm_deallocate(target_task, remote_string_addr, remote_string_size);
        return -1;
    }

    // 创建远程线程执行 dlopen
    thread_act_t remote_thread;
    arm_thread_state64_t thread_state;
    mach_msg_type_number_t thread_state_count = ARM_THREAD_STATE64_COUNT;

    __builtin_memset(&thread_state, 0, sizeof(thread_state));
    thread_state.__pc = (uint64_t)dlopen_addr;
    thread_state.__x[0] = remote_string_addr;
    thread_state.__x[1] = RTLD_NOW;

    kr = thread_create_running(target_task, ARM_THREAD_STATE64, (thread_state_t)&thread_state, thread_state_count, &remote_thread);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "无法创建远程线程\n");
        mach_vm_deallocate(target_task, remote_string_addr, remote_string_size);
        return -1;
    }

    // 等待远程线程完成
    mach_msg_type_number_t new_thread_state_count = ARM_THREAD_STATE64_COUNT;
    kr = thread_get_state(remote_thread, ARM_THREAD_STATE64, (thread_state_t)&thread_state, &new_thread_state_count);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "无法获取线程状态\n");
        } else {
        // 检查返回值
        uint64_t return_value = thread_state.__x[0];
        if (return_value == 0) {
            fprintf(stderr, "动态库注入失败\n");
        } else {
            printf("动态库成功注入\n");
        }
    }

    // 清理资源
    kr = thread_terminate(remote_thread);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "无法终止远程线程\n");
    }

    mach_vm_deallocate(target_task, remote_string_addr, remote_string_size);

    return (thread_state.__x[0] != 0) ? 0 : -1;
}

static void* mapper_thread_function(void* arg) {
    while (1) {
        pthread_mutex_lock(&requests_mutex);
        while (num_pending_requests == 0) {
            pthread_cond_wait(&request_cond, &requests_mutex);
        }

        MemoryRequest request = pending_requests[0];
        memmove(&pending_requests[0], &pending_requests[1], (num_pending_requests - 1) * sizeof(MemoryRequest));
        num_pending_requests--;

        pthread_mutex_unlock(&requests_mutex);

        void* data = read_memory(request.address, request.size);
        if (data != NULL) {
            memcpy(request.buffer, data, request.size);
            request.completed = 1;
        } else {
            request.completed = -1;
        }
    }
    return NULL;
}

static int queue_memory_request(vm_address_t address, size_t size, void* buffer) {
    pthread_mutex_lock(&requests_mutex);
    
    if (num_pending_requests >= MAX_PENDING_REQUESTS) {
        pthread_mutex_unlock(&requests_mutex);
        return -1;
    }

    MemoryRequest* request = &pending_requests[num_pending_requests];
    request->address = address;
    request->size = size;
    request->buffer = buffer;
    request->completed = 0;

    num_pending_requests++;
    pthread_cond_signal(&request_cond);

    pthread_mutex_unlock(&requests_mutex);
    return 0;
}

// 这里可以添加其他辅助函数,如错误处理、日志记录等

// 在 initialize_memory_module 函数中添加以下代码来创建mapper线程:
int initialize_memory_module(pid_t pid) {
    // ... (之前的代码)

    if (pthread_create(&mapper_thread, NULL, mapper_thread_function, NULL) != 0) {
        fprintf(stderr, "无法创建mapper线程\n");
        free(cached_regions);
        return -1;
    }

    return 0;
}

// 在 cleanup_memory_module 函数中添加以下代码来清理mapper线程:
void cleanup_memory_module() {
    // ... (之前的代码)

    pthread_cancel(mapper_thread);
    pthread_join(mapper_thread, NULL);

    pthread_mutex_destroy(&regions_mutex);
    pthread_mutex_destroy(&requests_mutex);
    pthread_cond_destroy(&request_cond);
}