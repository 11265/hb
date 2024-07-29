#ifndef MEMORY_MODULE_H
#define MEMORY_MODULE_H

#include <mach/mach.h>
#include <stdint.h>
#include <unistd.h>

#define INITIAL_CACHED_REGIONS 10
#define MAX_CACHED_REGIONS 100
#define MIN_CACHED_REGIONS 5
#define REGION_SIZE (1024 * 1024)  // 1MB
#define NUM_THREADS 4
#define MAX_PENDING_REQUESTS 100

typedef struct {
    vm_address_t base_address;
    void* mapped_memory;
    vm_size_t mapped_size;
    uint32_t access_count;
    time_t last_access;
} MemoryRegion;

typedef struct {
    vm_address_t address;
    size_t size;
    void* result;
    int is_write;
    int32_t write_value;
} MemoryRequest;

// 初始化内存模块
int 初始化内存模块(pid_t pid);

// 关闭内存模块
void 关闭内存模块();

// 读取内存
int32_t 读内存i32(vm_address_t address);
int64_t 读内存i64(vm_address_t address);
float   读内存f32(vm_address_t address);
double 读内存f64(vm_address_t address);

// 写入内存
int 写内存i32(vm_address_t address, int32_t value);
int 写内存i64(vm_address_t address, int64_t value);
int 写内存f32(vm_address_t address, float value);
int 写内存f64(vm_address_t address, double value);

// 新增函数声明
void*   读任意地址(vm_address_t address, size_t size);
int     写任意地址(vm_address_t address, const void* data, size_t size);

#endif // MEMORY_MODULE_H