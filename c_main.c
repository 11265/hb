#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>

#define TARGET_PID 22496  // 替换为目标进程的 PID
#define TARGET_ADDRESS 0x102DD2404  // 替换为目标内存地址

int c_main(void) {
    if (initialize_memory_access(TARGET_PID, TARGET_ADDRESS, 4096) != 0) {
        fprintf(stderr, "无法初始化内存访问\n");
        return -1;
    }

    int32_t int_value = 读内存i32(TARGET_ADDRESS);
    printf("读取的 int32_t 值: %d\n", int_value);

    cleanup_memory_access();
    return 0;
}