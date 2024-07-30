// 内存模块.h

#ifndef MEMORY_MODULE_H
#define MEMORY_MODULE_H

#include <mach/mach.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>

#define INITIAL_CACHED_REGIONS 100
#define MEMORY_POOL_SIZE (10 * 1024 * 1024)  // 10MB 内存池
#define SMALL_ALLOCATION_THRESHOLD 256  // 小于此值的分配使用内存池

typedef struct MemoryChunk {
    struct MemoryChunk* next;
    size_t size;
} MemoryChunk;

typedef struct {
    void* pool;
    MemoryChunk* free_list;
} MemoryPool;

extern MemoryPool memory_pool;

typedef struct {
    vm_address_t base_address;
    void* mapped_memory;
    size_t mapped_size;
    uint32_t access_count;
    time_t last_access;
} MemoryRegion;

typedef struct {
    void* data;
    int from_pool;
} MemoryReadResult;

void 初始化内存池(MemoryPool* pool);
void* 内存池分配(MemoryPool* pool, size_t size);
void 内存池释放(MemoryPool* pool, void* ptr);
void 销毁内存池(MemoryPool* pool);

MemoryReadResult 读任意地址(vm_address_t address, size_t size);
int 写任意地址(vm_address_t address, const void* data, size_t size);

int32_t 读内存i32(vm_address_t address);
int64_t 读内存i64(vm_address_t address);
float   读内存f32(vm_address_t address);
double  读内存f64(vm_address_t address);

int 写内存i32(vm_address_t address, int32_t value);
int 写内存i64(vm_address_t address, int64_t value);
int 写内存f32(vm_address_t address, float value);
int 写内存f64(vm_address_t address, double value);

int 初始化内存模块(pid_t pid);
void 关闭内存模块();

#endif // MEMORY_MODULE_H