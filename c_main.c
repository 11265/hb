#include <mach/mach.h>
#include <stdio.h>
#include <stdlib.h>

#define BLOCK_SIZE 4096

static mach_port_t task_port;
static vm_address_t mapped_address = 0;
static vm_size_t mapped_size = 0;

int initialize_memory_access(pid_t pid, mach_vm_address_t target_address, vm_size_t size) {
    kern_return_t kr = task_for_pid(mach_task_self(), pid, &task_port);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "task_for_pid failed: %s\n", mach_error_string(kr));
        return -1;
    }

    // 将目标进程的内存映射到当前进程
    kr = mach_vm_map(mach_task_self(), &mapped_address, size, 0, VM_FLAGS_ANYWHERE,
                     task_port, target_address, FALSE, VM_PROT_READ, VM_PROT_READ, VM_INHERIT_NONE);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "mach_vm_map failed: %s\n", mach_error_string(kr));
        return -1;
    }

    mapped_size = size;
    return 0;
}

void cleanup_memory_access() {
    if (mapped_address != 0) {
        mach_vm_deallocate(mach_task_self(), mapped_address, mapped_size);
        mapped_address = 0;
        mapped_size = 0;
    }
    mach_port_deallocate(mach_task_self(), task_port);
}

void* read_memory_block(mach_vm_address_t address, size_t size) {
    if (address < mapped_address || address + size > mapped_address + mapped_size) {
        fprintf(stderr, "Address out of mapped range\n");
        return NULL;
    }
    return (void*)(mapped_address + (address - mapped_address));
}

// 使用示例
int main() {
    pid_t target_pid = 1234; // 替换为目标进程的PID
    mach_vm_address_t target_address = 0x10000000; // 替换为目标内存地址
    vm_size_t map_size = BLOCK_SIZE;

    if (initialize_memory_access(target_pid, target_address, map_size) != 0) {
        return -1;
    }

    // 读取int32_t
    int32_t* int_value = (int32_t*)read_memory_block(target_address, sizeof(int32_t));
    if (int_value) {
        printf("Read int32_t: %d\n", *int_value);
    }

    // 读取double
    double* double_value = (double*)read_memory_block(target_address + sizeof(int32_t), sizeof(double));
    if (double_value) {
        printf("Read double: %f\n", *double_value);
    }

    cleanup_memory_access();
    return 0;
}