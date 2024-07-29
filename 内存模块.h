#ifndef MEMORY_MODULE_H
#define MEMORY_MODULE_H

#include <mach/mach.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>

#define REGION_SIZE (1024 * 1024)  // 1MB
#define INITIAL_CACHED_REGIONS 10
#define MIN_CACHED_REGIONS 5
#define MAX_CACHED_REGIONS 100
#define MAX_PENDING_REQUESTS 100

typedef struct {
    vm_address_t base_address;
    void *mapped_memory;
    vm_size_t mapped_size;
    uint32_t access_count;
    time_t last_access;
} MemoryRegion;

typedef struct {
    vm_address_t address;
    void *result;
    pthread_cond_t *cond;
    pthread_mutex_t *mutex;
    int *completed;
    int is_write;
    char write_value[sizeof(double)];  // 足够大的缓冲区来存储任何类型
} MemoryRequest;

int initialize_memory_module(pid_t pid);
void cleanup_memory_module(void);

// 同步读取函数
int32_t 读内存i32(vm_address_t address);
int64_t 读内存i64(vm_address_t address);
float 	读内存float(vm_address_t address);
double 	读内存double(vm_address_t address);

// 异步读取函数
int32_t 异步读内存i32(vm_address_t address);
int64_t 异步读内存i64(vm_address_t address);
float 	异步读内存float(vm_address_t address);
double 	异步读内存double(vm_address_t address);

// 同步写入函数
int 写内存i32(vm_address_t address, int32_t value);
int 写内存i64(vm_address_t address, int64_t value);
int 写内存float(vm_address_t address, float value);
int 写内存double(vm_address_t address, double value);

// 异步写入函数
int 异步写内存i32(vm_address_t address, int32_t value);
int 异步写内存i64(vm_address_t address, int64_t value);
int 异步写内存float(vm_address_t address, float value);
int 异步写内存double(vm_address_t address, double value);

#endif // MEMORY_MODULE_H