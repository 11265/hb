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
#define NUM_THREADS 4

typedef struct {
    vm_address_t base_address;
    void *mapped_memory;
    vm_size_t mapped_size;
    uint32_t access_count;
    time_t last_access;
} __attribute__((aligned(4))) MemoryRegion;

typedef struct {
    vm_address_t address;
    void *result;
    pthread_cond_t *cond;
    pthread_mutex_t *mutex;
    int *completed;
    int is_write;
    char write_value[8];
} __attribute__((aligned(4))) MemoryRequest;

int initialize_memory_module(pid_t pid);
void cleanup_memory_module(void);

// 同步读取函数
int32_t 读内存i32(vm_address_t address);
int64_t 读内存i64(vm_address_t address);
float   读内存f32(vm_address_t address);
double  读内存f64(vm_address_t address);

// 同步写入函数
int 写内存i32(vm_address_t address, int32_t value);
int 写内存i64(vm_address_t address, int64_t value);
int 写内存f32(vm_address_t address, float value);
int 写内存f64(vm_address_t address, double value);

#endif // MEMORY_MODULE_H