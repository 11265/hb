#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <mach/mach.h>
#include <mach/vm_map.h>

#define MY_PAGE_SIZE 4096

// 这个函数需要在内核扩展中实现
extern kern_return_t replace_pte(task_t src_task, vm_address_t src_addr, 
                                 task_t dst_task, vm_address_t dst_addr);

static task_t target_task;

int initialize_memory_module(pid_t target_pid) {
    kern_return_t kr = task_for_pid(mach_task_self(), target_pid, &target_task);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "Failed to get task for pid %d: %s\n", target_pid, mach_error_string(kr));
        return -1;
    }
    return 0;
}

void* read_memory(vm_address_t target_addr, size_t size) {
    vm_address_t local_addr;
    kern_return_t kr;

    // 在本地进程中分配内存
    kr = vm_allocate(mach_task_self(), &local_addr, size, VM_FLAGS_ANYWHERE);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "Failed to allocate local memory: %s\n", mach_error_string(kr));
        return NULL;
    }

    // 替换PTE
    for (vm_address_t offset = 0; offset < size; offset += MY_PAGE_SIZE) {
        kr = replace_pte(mach_task_self(), local_addr + offset,
                         target_task, target_addr + offset);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "Failed to replace PTE: %s\n", mach_error_string(kr));
            vm_deallocate(mach_task_self(), local_addr, size);
            return NULL;
        }
    }

    // 现在local_addr直接映射到目标进程的内存
    void* buffer = malloc(size);
    if (!buffer) {
        vm_deallocate(mach_task_self(), local_addr, size);
        return NULL;
    }

    memcpy(buffer, (void*)local_addr, size);
    vm_deallocate(mach_task_self(), local_addr, size);

    return buffer;
}

int write_memory(vm_address_t target_addr, const void* data, size_t size) {
    vm_address_t local_addr;
    kern_return_t kr;

    // 在本地进程中分配内存
    kr = vm_allocate(mach_task_self(), &local_addr, size, VM_FLAGS_ANYWHERE);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "Failed to allocate local memory: %s\n", mach_error_string(kr));
        return -1;
    }

    // 替换PTE
    for (vm_address_t offset = 0; offset < size; offset += MY_PAGE_SIZE) {
        kr = replace_pte(mach_task_self(), local_addr + offset,
                         target_task, target_addr + offset);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "Failed to replace PTE: %s\n", mach_error_string(kr));
            vm_deallocate(mach_task_self(), local_addr, size);
            return -1;
        }
    }

    // 直接写入，实际上是写入目标进程的内存
    memcpy((void*)local_addr, data, size);

    vm_deallocate(mach_task_self(), local_addr, size);
    return 0;
}

// 使用示例
int c_main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <target_pid>\n", argv[0]);
        return 1;
    }

    pid_t target_pid = atoi(argv[1]);
    if (initialize_memory_module(target_pid) != 0) {
        return 1;
    }

    // 读取示例
    vm_address_t target_addr = 0x1000; // 示例地址
    size_t read_size = 100;
    void* data = read_memory(target_addr, read_size);
    if (data) {
        printf("Read %zu bytes from address 0x%lx\n", read_size, target_addr);
        free(data);
    }

    // 写入示例
    char write_data[] = "Hello, target process!";
    if (write_memory(target_addr, write_data, strlen(write_data) + 1) == 0) {
        printf("Wrote data to address 0x%lx\n", target_addr);
    }

    return 0;
}
