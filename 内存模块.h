#ifndef MEMORY_MODULE_H
#define MEMORY_MODULE_H

#include <mach/mach.h>
#include <stdint.h>
#include <unistd.h>

#define INITIAL_CACHED_REGIONS 100
#define NUM_THREADS 4
#define MAX_PENDING_REQUESTS 1000

typedef struct {
    vm_address_t base_address;
    void* mapped_memory;
    size_t mapped_size;
    uint32_t access_count;
    time_t last_access;
} MemoryRegion;

typedef struct {
    vm_address_t address;
    size_t size;
    void* buffer;
    int operation; // 0 for read, 1 for write
    void* result;  // 用于存储操作结果
} MemoryRequest;

int  初始化内存模块(pid_t pid);
void 关闭内存模块();

void* 读任意地址(vm_address_t address, size_t size);
int   写任意地址(vm_address_t address, const void* data, size_t size);

int32_t 读内存i32(vm_address_t address);
int64_t 读内存i64(vm_address_t address);
float   读内存f32(vm_address_t address);
double  读内存f64(vm_address_t address);

int 写内存i32(vm_address_t address, int32_t value);
int 写内存i64(vm_address_t address, int64_t value);
int 写内存f32(vm_address_t address, float value);
int 写内存f64(vm_address_t address, double value);

MemoryRegion* get_or_create_page(vm_address_t address);
void* 处理内存请求(void* arg);

#endif // MEMORY_MODULE_H