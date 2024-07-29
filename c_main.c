#include <mach/mach.h>
#include <stdio.h>
#include <stdlib.h>

static mach_port_t task_port;

int initialize_memory_access(pid_t pid) {
    kern_return_t kr = task_for_pid(mach_task_self(), pid, &task_port);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "task_for_pid 失败: %s\n", mach_error_string(kr));
        return -1;
    }
    return 0;
}

void cleanup_memory_access() {
    mach_port_deallocate(mach_task_self(), task_port);
}

int read_memory(mach_vm_address_t address, void *buffer, size_t size) {
    vm_size_t data_size = size;
    kern_return_t kr = vm_read_overwrite(task_port, address, size, (vm_address_t)buffer, &data_size);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "vm_read_overwrite 失败: %s\n", mach_error_string(kr));
        return -1;
    }
    return 0;
}

// 使用示例
int main() {
    pid_t target_pid = 1234; // 替换为目标进程的 PID
    mach_vm_address_t target_address = 0x10000000; // 替换为目标内存地址

    if (initialize_memory_access(target_pid) != 0) {
        return -1;
    }

    // 读取 int32_t
    int32_t int_value;
    if (read_memory(target_address, &int_value, sizeof(int32_t)) == 0) {
        printf("读取的 int32_t 值: %d\n", int_value);
    }

    // 读取 double
    double double_value;
    if (read_memory(target_address + sizeof(int32_t), &double_value, sizeof(double)) == 0) {
        printf("读取的 double 值: %f\n", double_value);
    }

    cleanup_memory_access();
    return 0;
}