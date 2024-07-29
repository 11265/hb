#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>
#include <mach/mach.h>

#define TARGET_PID 23569

int c_main(void) {
    printf("开始初始化内存访问...\n");

    int result = initialize_memory_module(TARGET_PID);
    if (result != 0) {
        fprintf(stderr, "无法初始化内存模块，错误代码：%d\n", result);
        return -1;
    }
    printf("内存模块初始化成功\n");

    // 读取多个地址的示例
    vm_address_t addresses[] = {0x104bb7c78, 0x104cc8000, 0x104dd9000, 0x104bb7c7c, 0x104bb7c80};
    for (int i = 0; i < sizeof(addresses) / sizeof(vm_address_t); i++) {
        printf("尝试异步读取地址 0x%llx\n", (unsigned long long)addresses[i]);
        int32_t value = 异步读内存i32(addresses[i]);
        printf("地址 0x%llx 处异步读取的 int32_t 值: %d (0x%x)\n", 
               (unsigned long long)addresses[i], value, value);
    }

    cleanup_memory_module();
    printf("清理完成\n");
    return 0;
}