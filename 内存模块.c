#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <mach/mach.h>
#include <mach/vm_map.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

static void *mapped_memory = NULL;
static size_t mapped_size = 0;
static vm_address_t base_address = 0;

int initialize_memory_access(pid_t pid, vm_address_t address, size_t size) {
    task_t task;
    kern_return_t kr = task_for_pid(mach_task_self(), pid, &task);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "task_for_pid failed: %s\n", mach_error_string(kr));
        return -1;
    }

    vm_size_t region_size = size;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object_name;

    kr = vm_region_64(task, &address, &region_size, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info, &info_count, &object_name);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "vm_region_64 failed: %s\n", mach_error_string(kr));
        return -1;
    }

    mapped_memory = mmap(NULL, region_size, PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (mapped_memory == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        return -1;
    }

    kr = vm_read_overwrite(task, address, region_size, (vm_address_t)mapped_memory, &region_size);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "vm_read_overwrite failed: %s\n", mach_error_string(kr));
        munmap(mapped_memory, region_size);
        return -1;
    }

    mapped_size = region_size;
    base_address = address;

    return 0;
}

void cleanup_memory_access() {
    if (mapped_memory != NULL) {
        munmap(mapped_memory, mapped_size);
        mapped_memory = NULL;
        mapped_size = 0;
        base_address = 0;
    }
}

static void* get_mapped_address(vm_address_t address) {
    if (mapped_memory == NULL || address < base_address || address >= base_address + mapped_size) {
        return NULL;
    }
    return (void*)((char*)mapped_memory + (address - base_address));
}

// 读取函数
int32_t 读内存i32(vm_address_t address) {
    void* mapped_addr = get_mapped_address(address);
    if (mapped_addr == NULL) return 0;
    return *(int32_t*)mapped_addr;
}

int64_t 读内存i64(vm_address_t address) {
    void* mapped_addr = get_mapped_address(address);
    if (mapped_addr == NULL) return 0;
    return *(int64_t*)mapped_addr;
}

float 读内存f32(vm_address_t address) {
    void* mapped_addr = get_mapped_address(address);
    if (mapped_addr == NULL) return 0.0f;
    return *(float*)mapped_addr;
}

double 读内存f64(vm_address_t address) {
    void* mapped_addr = get_mapped_address(address);
    if (mapped_addr == NULL) return 0.0;
    return *(double*)mapped_addr;
}