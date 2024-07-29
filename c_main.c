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

    // 查找 "pvz" 模块的基地址
    vm_address_t base_address = find_module_base(target_task, "pvz");
    if (base_address == 0) {
        fprintf(stderr, "未找到 pvz 模块\n");
        cleanup_memory_module();
        return -1;
    }
    printf("pvz 模块基地址: 0x%llx\n", (unsigned long long)base_address);

    // 读取多层指针
    int64_t final_address = read_multi_level_pointer(target_task, base_address, 2, 
                                                     0x20A7AA0, 0x400);
    
    if (final_address != 0) {
        int32_t value = 异步读内存i32(final_address);
        printf("多层指针读取结果: 0x%llx, 值: %d (0x%x)\n", 
               (unsigned long long)final_address, value, value);
    }

    cleanup_memory_module();
    printf("清理完成\n");
    return 0;
}