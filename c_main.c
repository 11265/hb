#include "内存模块.h"
#include "查找进程.h"
#include "模块基地址.h"
#include <stdio.h>
#include <stdint.h>
#include <mach/mach.h>

#define TARGET_PROCESS_NAME "pvz"  // 定义目标进程名称

int c_main(void) {
    pid_t target_pid = get_pid_by_name(TARGET_PROCESS_NAME);  // 获取目标进程的PID
    if (target_pid == -1) {
        fprintf(stderr, "未找到进程：%s\n", TARGET_PROCESS_NAME);
        return -1;
    }
    
    printf("找到进程 %s，PID: %d\n", TARGET_PROCESS_NAME, target_pid);

    int result = initialize_memory_module(target_pid);  // 初始化内存模块
    if (result != 0) {
        fprintf(stderr, "无法初始化内存模块，错误代码：%d\n", result);
        return -1;
    }
    printf("内存模块初始化成功\n");

    task_t target_task;
    kern_return_t kr = task_for_pid(mach_task_self(), target_pid, &target_task);  // 获取目标进程的task
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "无法获取目标进程的 task，错误码: %d\n", kr);
        cleanup_memory_module();
        return -1;
    }
    printf("成功获取目标进程的 task\n");

    vm_address_t base_address = find_module_base(target_task, "pvz");  // 查找"pvz"模块的基地址
    if (base_address == 0) {
        fprintf(stderr, "未找到 pvz 模块\n");
        mach_port_deallocate(mach_task_self(), target_task);
        cleanup_memory_module();
        return -1;
    }
    printf("pvz 模块基地址: 0x%llx\n", (unsigned long long)base_address);

    // 定义偏移量
    int64_t offset1 = 0x20A7AA0;
    int64_t offset2 = 0x400;  // 修正为 0x400
    int num_offsets = 2;

    // 读取多级指针
    int64_t final_address = read_multi_level_pointer(target_task, base_address, num_offsets, offset1, offset2);
    
    if (final_address == 0) {
        fprintf(stderr, "无法读取多级指针\n");
        mach_port_deallocate(mach_task_self(), target_task);
        cleanup_memory_module();
        return -1;
    }

    printf("最终地址: 0x%llx\n", (unsigned long long)final_address);

    // 读取 32 位整数值
    int32_t value;
    int read_result = 读内存i32(final_address, &value);
    if (read_result != 0) {
        fprintf(stderr, "读取内存失败，错误代码：%d\n", read_result);
        mach_port_deallocate(mach_task_self(), target_task);
        cleanup_memory_module();
        return -1;
    }

    printf("读取的值: %d (0x%x)\n", value, value);

    mach_port_deallocate(mach_task_self(), target_task);  // 释放目标进程的task

    cleanup_memory_module();  // 清理内存模块
    printf("清理完成\n");
    return 0;
}