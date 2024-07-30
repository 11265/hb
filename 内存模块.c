// 内存模块.c

#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define ALIGN4(size) (((size) + 3) & ~3)
#define PAGE_SIZE 4096
#define PAGE_MASK (~(PAGE_SIZE - 1))

static MemoryRegion *cached_regions = NULL;
static int num_cached_regions = 0;
static int max_cached_regions = INITIAL_CACHED_REGIONS;
static pid_t target_pid;
static task_t target_task;

static pthread_mutex_t regions_mutex = PTHREAD_MUTEX_INITIALIZER;

MemoryPool memory_pool;
pthread_mutex_t memory_pool_mutex = PTHREAD_MUTEX_INITIALIZER;

void 初始化内存池(MemoryPool* pool) {
    pool->pool = malloc(MEMORY_POOL_SIZE);
    if (pool->pool) {
        pool->free_list = (MemoryChunk*)pool->pool;
        pool->free_list->next = NULL;
        pool->free_list->size = MEMORY_POOL_SIZE - sizeof(MemoryChunk);
    }
}

void* 内存池分配(MemoryPool* pool, size_t size) {
    pthread_mutex_lock(&memory_pool_mutex);
    size = ALIGN4(size + sizeof(MemoryChunk));
    
    MemoryChunk* prev = NULL;
    MemoryChunk* chunk = pool->free_list;
    
    while (chunk != NULL) {
        if (chunk->size >= size) {
            if (chunk->size > size + sizeof(MemoryChunk)) {
                MemoryChunk* new_chunk = (MemoryChunk*)((char*)chunk + size);
                new_chunk->size = chunk->size - size;
                new_chunk->next = chunk->next;
                chunk->size = size;
                
                if (prev == NULL) {
                    pool->free_list = new_chunk;
                } else {
                    prev->next = new_chunk;
                }
            } else {
                if (prev == NULL) {
                    pool->free_list = chunk->next;
                } else {
                    prev->next = chunk->next;
                }
            }
            
            chunk->next = NULL;
            pthread_mutex_unlock(&memory_pool_mutex);
            return (char*)chunk + sizeof(MemoryChunk);
        }
        
        prev = chunk;
        chunk = chunk->next;
    }
    
    pthread_mutex_unlock(&memory_pool_mutex);
    return NULL;
}

void 内存池释放(MemoryPool* pool, void* ptr) {
    if (!ptr) return;
    
    pthread_mutex_lock(&memory_pool_mutex);
    MemoryChunk* chunk = (MemoryChunk*)((char*)ptr - sizeof(MemoryChunk));
    chunk->next = pool->free_list;
    pool->free_list = chunk;
    pthread_mutex_unlock(&memory_pool_mutex);
}

void 销毁内存池(MemoryPool* pool) {
    if (pool && pool->pool) {
        free(pool->pool);
        pool->pool = NULL;
        pool->free_list = NULL;
    }
}

static MemoryRegion* get_or_create_page(vm_address_t address) {
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

MemoryReadResult 读任意地址(vm_address_t address, size_t size) {
    MemoryReadResult result = {NULL, 0};
    MemoryRegion* region = get_or_create_page(address);
    if (!region) {
        return result;
    }
    
    size_t offset = address - region->base_address;
    void* buffer;
    int use_memory_pool = (size <= SMALL_ALLOCATION_THRESHOLD);
    if (use_memory_pool) {
        buffer = 内存池分配(&memory_pool, size);
    }
    if (!buffer) {
        buffer = malloc(size);
        use_memory_pool = 0;
    }
    
    if (!buffer) {
        return result;
    }
    
    if (offset + size > PAGE_SIZE) {
        size_t first_part = PAGE_SIZE - offset;
        memcpy(buffer, (char*)region->mapped_memory + offset, first_part);
        
        size_t remaining = size - first_part;
        MemoryReadResult remaining_result = 读任意地址(address + first_part, remaining);
        if (!remaining_result.data) {
            if (use_memory_pool) {
                内存池释放(&memory_pool, buffer);
            } else {
                free(buffer);
            }
            return result;
        }
        
        memcpy((char*)buffer + first_part, remaining_result.data, remaining);
        if (remaining_result.from_pool) {
            内存池释放(&memory_pool, remaining_result.data);
        } else {
            free(remaining_result.data);
        }
    } else {
        memcpy(buffer, (char*)region->mapped_memory + offset, size);
    }
    
    result.data = buffer;
    result.from_pool = use_memory_pool;
    return result;
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
        
        // 写入目标进程内存
        kern_return_t kr = vm_write(target_task, address, (vm_offset_t)data, first_part);
        if (kr != KERN_SUCCESS) {
            return -1;
        }
        
        size_t remaining = size - first_part;
        return 写任意地址(address + first_part, (char*)data + first_part, remaining);
    } else {
        memcpy((char*)region->mapped_memory + offset, data, size);
        
        // 写入目标进程内存
        kern_return_t kr = vm_write(target_task, address, (vm_offset_t)data, size);
        if (kr != KERN_SUCCESS) {
            return -1;
        }
        
        return 0;
    }
}

int32_t 读内存i32(vm_address_t address) {
    MemoryReadResult result = 读任意地址(address, sizeof(int32_t));
    if (!result.data) {
        return 0;
    }
    int32_t value = *(int32_t*)result.data;
    if (result.from_pool) {
        内存池释放(&memory_pool, result.data);
    } else {
        free(result.data);
    }
    return value;
}

int64_t 读内存i64(vm_address_t address) {
    MemoryReadResult result = 读任意地址(address, sizeof(int64_t));
    if (!result.data) {
        return 0;
    }
    int64_t value = *(int64_t*)result.data;
    if (result.from_pool) {
        内存池释放(&memory_pool, result.data);
    } else {
        free(result.data);
    }
    return value;
}

float 读内存f32(vm_address_t address) {
    MemoryReadResult result = 读任意地址(address, sizeof(float));
    if (!result.data) {
        return 0.0f;
    }
    float value = *(float*)result.data;
    if (result.from_pool) {
        内存池释放(&memory_pool, result.data);
    } else {
        free(result.data);
    }
    return value;
}

double 读内存f64(vm_address_t address) {
    MemoryReadResult result = 读任意地址(address, sizeof(double));
    if (!result.data) {
        return 0.0;
    }
    double value = *(double*)result.data;
    if (result.from_pool) {
        内存池释放(&memory_pool, result.data);
    } else {
        free(result.data);
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

int 初始化内存模块(pid_t pid) {
    target_pid = pid;
    kern_return_t kr = task_for_pid(mach_task_self(), target_pid, &target_task);
    if (kr != KERN_SUCCESS) {
        return -1;
    }
    
    cached_regions = malloc(sizeof(MemoryRegion) * max_cached_regions);
    if (!cached_regions) {
        return -1;
    }
    
    初始化内存池(&memory_pool);
    
    // 修改目标进程的内存保护
    vm_address_t address = 0;
    vm_size_t size = 0;
    natural_t depth = 0;
    while (1) {
        struct vm_region_submap_info_64 info;
        mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
        kr = vm_region_recurse_64(target_task, &address, &size, &depth, (vm_region_info_64_t)&info, &count);
        if (kr != KERN_SUCCESS) break;

        if (!(info.protection & VM_PROT_WRITE)) {
            vm_prot_t new_protection = info.protection | VM_PROT_WRITE;
            kr = vm_protect(target_task, address, size, FALSE, new_protection);
            if (kr != KERN_SUCCESS) {
                // 处理错误
                printf("Failed to modify memory protection at address 0x%llx, size: %llu, error: %d\n", 
                       (unsigned long long)address, (unsigned long long)size, kr);
                // 注意：这里我们继续执行，而不是立即返回，因为我们可能还能修改其他内存区域
            } else {
                printf("Successfully modified memory protection at address 0x%llx, size: %llu\n", 
                       (unsigned long long)address, (unsigned long long)size);
            }
        }

        address += size;
    }

    return 0;
}

void 关闭内存模块() {
    for (int i = 0; i < num_cached_regions; i++) {
        munmap(cached_regions[i].mapped_memory, cached_regions[i].mapped_size);
    }
    
    free(cached_regions);
    销毁内存池(&memory_pool);
}