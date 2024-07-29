#include "内存模块.h"                                           // 包含自定义的头文件
#include <stdio.h>                                              // 标准输入输出
#include <stdlib.h>                                             // 标准库函数
#include <mach/mach_error.h>                                    // Mach错误处理
#include <stdbool.h>                                            // 布尔类型支持

static mach_port_t global_task_port;                            // 全局任务端口
static bool is_task_port_initialized = false;                   // 任务端口初始化标志

int initialize_task_port(pid_t pid) {
    if (is_task_port_initialized) {                             // 如果已经初始化，直接返回成功
        return KERN_SUCCESS;
    }

    kern_return_t kret = task_for_pid(mach_task_self(), pid, &global_task_port);  // 获取目标进程的任务端口
    if (kret != KERN_SUCCESS) {                                 // 如果获取失败，打印错误信息
        printf("task_for_pid 失败: %s\n", mach_error_string(kret));
        return kret;
    }
    is_task_port_initialized = true;                            // 设置初始化标志
    return KERN_SUCCESS;                                        // 返回成功
}

void cleanup_task_port() {
    if (is_task_port_initialized) {                             // 如果已初始化
        mach_port_deallocate(mach_task_self(), global_task_port);  // 释放端口
        is_task_port_initialized = false;                       // 重置初始化标志
    }
}

static kern_return_t read_memory_from_task(mach_vm_address_t address, size_t size, void *buffer) {
    vm_size_t data_size = size;
    kern_return_t kret = vm_read_overwrite(global_task_port, address, size, (vm_address_t)buffer, &data_size);  // 使用vm_read_overwrite读取内存
    if (kret != KERN_SUCCESS) {                                 // 如果读取失败，打印错误信息
        printf("vm_read_overwrite 失败: %s\n", mach_error_string(kret));
        return kret;
    }
    return KERN_SUCCESS;                                        // 返回成功
}

int 读内存i32(mach_vm_address_t address, int32_t *value) {
    if (!is_task_port_initialized) {                            // 如果任务端口未初始化，返回错误
        return -1;
    }

    kern_return_t kret = read_memory_from_task(address, sizeof(int32_t), value);  // 读取内存
    if (kret != KERN_SUCCESS) {                                 // 如果读取失败，返回错误
        return -1;
    }
    return 0;                                                   // 返回成功
}

int 读内存i64(mach_vm_address_t address, int64_t *value) {
    if (!is_task_port_initialized) {                            // 如果任务端口未初始化，返回错误
        return -1;
    }

    kern_return_t kret = read_memory_from_task(address, sizeof(int64_t), value);  // 读取内存
    if (kret != KERN_SUCCESS) {                                 // 如果读取失败，返回错误
        return -1;
    }
    return 0;                                                   // 返回成功
}