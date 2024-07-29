#ifndef MEMORY_MODULE_H
#define MEMORY_MODULE_H

#include <mach/mach.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>

#define INITIAL_CACHED_REGIONS 10
#define MAX_CACHED_REGIONS 50
#define MIN_CACHED_REGIONS 5
#define REGION_SIZE 0x100000  // 1MB
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
    int32_t *result;
    pthread_cond_t *cond;
    pthread_mutex_t *mutex;
    int *completed;
    int is_write;
    int32_t write_value;
} MemoryRequest;

int initialize_memory_module(pid_t pid);
void cleanup_memory_module(void);
int32_t 读内存i32(vm_address_t address);
int32_t 异步读内存i32(vm_address_t address);
int 写内存i32(vm_address_t address, int32_t value);
int 异步写内存i32(vm_address_t address, int32_t value);

#endif // MEMORY_MODULE_H