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
static mach_vm_size_t mapped_size = 0;
static mach_vm_address_t base_address = 0;
static task_t target_task = MACH_PORT_NULL;

int initialize_memory_access(pid_t pid, mach_vm_address_t address, mach_vm_size_t size) {
    kern_return_t kr;

    // 获取目标进程的task
    kr = task_for_pid(mach_task_self(), pid, &target_task);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "task_for_pid failed: %s (err: %d)\n", mach_error_string(kr), kr);
        return -1;
    }

    // 获取内存区域信息
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object_name;

    kr = mach_vm_region(target_task, &address, &size, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info, &info_count, &object_name);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "mach_vm_region failed: %s (err: %d)\n", mach_error_string(kr), kr);
        return -1;
    }

    // 分配本地内存
    mapped_memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (mapped_memory == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s (errno: %d)\n", strerror(errno), errno);
        return -1;
    }

    // 读取目标进程内存
    mach_vm_size_t bytes_read;
    kr = mach_vm_read_overwrite(target_task, address, size, (mach_vm_address_t)mapped_memory, &bytes_read);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "mach_vm_read_overwrite failed: %s (err: %d)\n", mach_error_string(kr), kr);
        munmap(mapped_memory, size);
        return -1;
    }

    mapped_size = bytes_read;
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
    if (target_task != MACH_PORT_NULL) {
        mach_port_deallocate(mach_task_self(), target_task);
        target_task = MACH_PORT_NULL;
    }
}

static void* get_mapped_address(mach_vm_address_t address) {
    if (mapped_memory == NULL || address < base_address || address >= base_address + mapped_size) {
        return NULL;
    }
    return (void*)((char*)mapped_memory + (address - base_address));
}

// 读取函数
int32_t 读内存i32(mach_vm_address_t address) {
    void* mapped_addr = get_mapped_address(address);
    if (mapped_addr == NULL) return 0;
    return *(int32_t*)mapped_addr;
}

int64_t 读内存i64(mach_vm_address_t address) {
    void* mapped_addr = get_mapped_address(address);
    if (mapped_addr == NULL) return 0;
    return *(int64_t*)mapped_addr;
}

float 读内存f32(mach_vm_address_t address) {
    void* mapped_addr = get_mapped_address(address);
    if (mapped_addr == NULL) return 0.0f;
    return *(float*)mapped_addr;
}

double 读内存f64(mach_vm_address_t address) {
    void* mapped_addr = get_mapped_address(address);
    if (mapped_addr == NULL) return 0.0;
    return *(double*)mapped_addr;
}