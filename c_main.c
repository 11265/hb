#include "内存模块.h"
#include "查找进程.h"
#include "模块基地址.h"
#include <stdio.h>
#include <stdint.h>
#include <mach/mach.h>

#define TARGET_PROCESS_NAME "pvz"

int c_main(void) {
    pid_t target_pid = get_pid_by_name(TARGET_PROCESS_NAME);
    if (target_pid == -1) {
        fprintf(stderr, "未找到进程：%s\n", TARGET_PROCESS_NAME);
        return -1;
    }
    
    printf("找到进程 %s，PID: %d\n", TARGET_PROCESS_NAME, target_pid);

    int result = initialize_memory_module(target_pid);
    if (result != 0) {
        fprintf(stderr, "无法初始化内存模块，错误代码：%d\n", result);
        return -1;
    }
    printf("内存模块初始化成功\n");

    // 获取目标进程的 task
    task_t target_task;
    kern_return_t kr = task_for_pid(mach_task_self(), target_pid, &target_task);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "无法获取目标进程的 task，错误码: %d\n", kr);
        cleanup_memory_module();
        return -1;
    }
    printf("成功获取目标进程的 task\n");

    // 查找 "pvz" 模块的基地址
    vm_address_t base_address = find_module_base(target_task, "pvz");
    if (base_address == 0) {
        fprintf(stderr, "未找到 pvz 模块\n");
        mach_port_deallocate(mach_task_self(), target_task);
        cleanup_memory_module();
        return -1;
    }
    printf("pvz 模块基地址: 0x%llx\n", (unsigned long long)base_address);

    // Define offsets
    int64_t offset1 = 0x20A7AA0;
    int64_t offset2 = 0x0;  // Replace with actual offset
    int num_offsets = 2;  // Number of offsets we're using

    // Read multi-level pointer
    int64_t final_value = read_multi_level_pointer(target_task, base_address, num_offsets, offset1, offset2);
    
    printf("最终值: %lld (0x%llx)\n", (long long)final_value, (unsigned long long)final_value);

    // If needed, you can process the final value further
    // For example, if this is a score or other value in the game
    printf("游戏中的值: %d\n", (int)final_value);

    // Read a 32-bit value from the final address
    int32_t value = 读内存i32(final_value);
    printf("读取的值: %d (0x%x)\n", value, value);

    // 释放目标进程的 task
    mach_port_deallocate(mach_task_self(), target_task);

    cleanup_memory_module();
    printf("清理完成\n");
    return 0;
}