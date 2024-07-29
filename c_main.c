#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>

#define TARGET_PID 22496  // 替换为目标进程的 PID
#define TARGET_ADDRESS 0x102DD2404  // 替换为目标内存地址
#define MAP_SIZE 4096  // 映射大小，可根据需要调整

int c_main(void) {
    if (initialize_memory_access(TARGET_PID, TARGET_ADDRESS, MAP_SIZE) != 0) {
        return -1;
    }

    // 读取 int32_t
    int32_t int_value = 读内存i32(TARGET_ADDRESS);
    printf("读取的 int32_t 值: %d\n", int_value);

    // 读取 double
    double double_value = 读内存f64(TARGET_ADDRESS + sizeof(int32_t));
    printf("读取的 double 值: %f\n", double_value);

    cleanup_memory_access();
    return 0;
}