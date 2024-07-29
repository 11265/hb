#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

static MemoryRegion cached_regions[MAX_CACHED_REGIONS] = {0};
static int num_cached_regions = 0;
static pid_t target_pid;
static task_t target_task;

static pthread_t mapper_thread;
static pthread_mutex_t regions_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t requests_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t request_cond = PTHREAD_COND_INITIALIZER;

static ReadRequest pending_requests[MAX_PENDING_REQUESTS];
static int num_pending_requests = 0;

static MemoryRegion* find_cached_region(vm_address_t address) {
    pthread_mutex_lock(&regions_mutex);
    for (int i = 0; i < num_cached_regions; i++) {
        if (address >= cached_regions[i].base_address && 
            address < cached_regions[i].base_address + cached_regions[i].mapped_size) {
            pthread_mutex_unlock(&regions_mutex);
            return &cached_regions[i];
        }
    }
    pthread_mutex_unlock(&regions_mutex);
    return NULL;
}

static int map_new_region(vm_address_t address) {
    kern_return_t kr;
    vm_address_t region_start = address & ~(REGION_SIZE - 1);
    vm_size_t size = REGION_SIZE;
    mach_vm_size_t bytes_read;

    pthread_mutex_lock(&regions_mutex);
    if (num_cached_regions >= MAX_CACHED_REGIONS) {
        munmap(cached_regions[0].mapped_memory, cached_regions[0].mapped_size);
        memmove(&cached_regions[0], &cached_regions[1], sizeof(MemoryRegion) * (MAX_CACHED_REGIONS - 1));
        num_cached_regions--;
    }

    void *mapped_memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (mapped_memory == MAP_FAILED) {
        pthread_mutex_unlock(&regions_mutex);
        perror("mmap 失败");
        return -1;
    }

    kr = vm_read_overwrite(target_task, region_start, size, (vm_address_t)mapped_memory, &bytes_read);
    if (kr != KERN_SUCCESS) {
        pthread_mutex_unlock(&regions_mutex);
        fprintf(stderr, "vm_read_overwrite 失败: %s\n", mach_error_string(kr));
        munmap(mapped_memory, size);
        return -2;
    }

    cached_regions[num_cached_regions].base_address = region_start;
    cached_regions[num_cached_regions].mapped_memory = mapped_memory;
    cached_regions[num_cached_regions].mapped_size = bytes_read;
    num_cached_regions++;

    pthread_mutex_unlock(&regions_mutex);

    printf("新映射内存区域：0x%llx - 0x%llx\n", 
           (unsigned long long)region_start, 
           (unsigned long long)(region_start + bytes_read));

    return 0;
}

static void* mapper_thread_func(void* arg) {
    while (1) {
        pthread_mutex_lock(&requests_mutex);
        while (num_pending_requests == 0) {
            pthread_cond_wait(&request_cond, &requests_mutex);
        }

        ReadRequest request = pending_requests[0];
        memmove(&pending_requests[0], &pending_requests[1], sizeof(ReadRequest) * (num_pending_requests - 1));
        num_pending_requests--;
        pthread_mutex_unlock(&requests_mutex);

        MemoryRegion* region = find_cached_region(request.address);
        if (region == NULL) {
            int result = map_new_region(request.address);
            if (result != 0) {
                fprintf(stderr, "无法映射内存区域，错误代码：%d\n", result);
                *request.result = 0;
            } else {
                region = find_cached_region(request.address);
            }
        }

        if (region != NULL) {
            vm_address_t offset = request.address - region->base_address;
            *request.result = *(int32_t *)((char *)region->mapped_memory + offset);
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

    printf("正在获取目标进程 (PID: %d) 的 task...\n", pid);
    kr = task_for_pid(mach_task_self(), pid, &target_task);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "无法获取目标进程的 task: %s\n", mach_error_string(kr));
        return -1;
    }
    printf("成功获取目标进程的 task\n");

    pthread_create(&mapper_thread, NULL, mapper_thread_func, NULL);

    return 0;
}

void cleanup_memory_module(void) {
    pthread_cancel(mapper_thread);
    pthread_join(mapper_thread, NULL);

    for (int i = 0; i < num_cached_regions; i++) {
        munmap(cached_regions[i].mapped_memory, cached_regions[i].mapped_size);
    }
    num_cached_regions = 0;
}

int32_t 读内存i32(vm_address_t address) {
    MemoryRegion* region = find_cached_region(address);
    if (region == NULL) {
        int result = map_new_region(address);
        if (result != 0) {
            fprintf(stderr, "无法映射内存区域，错误代码：%d\n", result);
            return 0;
        }
        region = find_cached_region(address);
    }

    if (address < region->base_address || address >= region->base_address + region->mapped_size - sizeof(int32_t)) {
        fprintf(stderr, "错误：地址 0x%llx 超出映射范围 (0x%llx - 0x%llx)\n", 
                (unsigned long long)address, 
                (unsigned long long)region->base_address, 
                (unsigned long long)(region->base_address + region->mapped_size));
        return 0;
    }

    vm_address_t offset = address - region->base_address;
    return *(int32_t *)((char *)region->mapped_memory + offset);
}

int32_t 异步读内存i32(vm_address_t address) {
    int32_t result = 0;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    int completed = 0;

    pthread_mutex_lock(&requests_mutex);
    if (num_pending_requests >= MAX_PENDING_REQUESTS) {
        pthread_mutex_unlock(&requests_mutex);
        fprintf(stderr, "等待请求队列已满\n");
        return 0;
    }

    pending_requests[num_pending_requests].address = address;
    pending_requests[num_pending_requests].result = &result;
    pending_requests[num_pending_requests].cond = &cond;
    pending_requests[num_pending_requests].mutex = &mutex;
    pending_requests[num_pending_requests].completed = &completed;
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