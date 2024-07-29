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
static vm_size_t mapped_size = 0;
static vm_address_t base_address = 0;
static task_t target_task = MACH_PORT_NULL;

int initialize_memory_access(pid_t pid, vm_address_t address, vm_size_t size) {
    kern_return_t kr;

    printf("正在获取目标进程 (PID: %d) 的 task...\n", pid);
    kr = task_for_pid(mach_task_self(), pid, &target_task);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "task_for_pid 失败: %s (err: %d)\n", mach_error_string(kr), kr);
        return -1;
    }
    printf("成功获取目标进程的 task\n");

    printf("正在获取内存区域信息...\n");
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object_name;

    kr = vm_region_64(target_task, &address, &size, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info, &info_count, &object_name);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "vm_region_64 失败: %s (err: %d)\n", mach_error_string(kr), kr);
        return -2;
    }
    printf("成功获取内存区域信息\n");
    printf("区域地址: 0x%llx, 大小: 0x%llx\n", (unsigned long long)address, (unsigned long long)size);

    printf("正在分配本地内存...\n");
    mapped_memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (mapped_memory == MAP_FAILED) {
        fprintf(stderr, "mmap 失败: %s (errno: %d)\n", strerror(errno), errno);
        return -3;
    }
    printf("成功分配本地内存\n");

    printf("正在读取目标进程内存...\n");
    vm_size_t bytes_read;
    kr = vm_read_overwrite(target_task, address, size, (vm_address_t)mapped_memory, &bytes_read);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "vm_read_overwrite 失败: %s (err: %d)\n", mach_error_string(kr), kr);
        munmap(mapped_memory, size);
        return -4;
    }
    printf("成功读取目标进程内存，读取了 %llu 字节\n", (unsigned long long)bytes_read);

    mapped_size = bytes_read;
    base_address = address;

    printf("映射内存范围：0x%llx - 0x%llx\n", (unsigned long long)base_address, (unsigned long long)(base_address + mapped_size));

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

static void* get_mapped_address(vm_address_t address) {
    if (mapped_memory == NULL || address < base_address || address >= base_address + mapped_size) {
        fprintf(stderr, "错误：地址 0x%llx 超出映射范围 (0x%llx - 0x%llx)\n", 
                (unsigned long long)address, 
                (unsigned long long)base_address, 
                (unsigned long long)(base_address + mapped_size));
        return NULL;
    }
    return (void*)((char*)mapped_memory + (address - base_address));
}

int32_t 读内存i32(vm_address_t address) {
    void* mapped_addr = get_mapped_address(address);
    if (mapped_addr == NULL) {
        fprintf(stderr, "错误：无法从地址 0x%llx 读取\n", (unsigned long long)address);
        return 0;
    }
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