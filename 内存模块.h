#ifndef MEMORY_MODULE_H
#define MEMORY_MODULE_H

#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <pthread.h>
#include <stdint.h>
#include <dlfcn.h>

#define INITIAL_CACHED_REGIONS 10
#define MAX_CACHED_REGIONS 100
#define MIN_CACHED_REGIONS 5
#define REGION_SIZE 4096
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
    void* buffer;
    int completed;
} MemoryRequest;

int initialize_memory_module(pid_t pid);
void cleanup_memory_module(void);

int32_t 异步读内存i32(vm_address_t address);
int64_t 异步读内存i64(vm_address_t address);
float 	异步读内存float(vm_address_t address);
double 	异步读内存double(vm_address_t address);

int 异步写内存i32(vm_address_t address, int32_t value);
int 异步写内存i64(vm_address_t address, int64_t value);
int 异步写内存float(vm_address_t address, float value);
int 异步写内存double(vm_address_t address, double value);

int 获取内存区域信息(MemoryRegion* regions, int max_regions);
int 注入动态库(const char* dylib_path);

#endif // MEMORY_MODULE_H