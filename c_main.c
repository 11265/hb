#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>

#define TARGET_PID 23569  // 替换为目标进程的 PID
#define BASE_ADDRESS 0x00000000  // 映射内存开始地址
#define MAP_SIZE 0x200000000  // 映射大小：0x200000000 - 0x00000000

int c_main(void) {
    if (initialize_memory_access(TARGET_PID, BASE_ADDRESS, MAP_SIZE) != 0) {
        fprintf(stderr, "无法初始化内存访问\n");
        return -1;
    }

    // 示例：读取某个地址的值（这里使用 0x102DD2404 作为示例）
    vm_address_t target_address = 0x104b94fc0;
    int32_t int_value = 读内存i32(target_address);
    printf("地址 0x%llx 处读取的 int32_t 值: %d\n", (unsigned long long)target_address, int_value);

    cleanup_memory_access();
    return 0;
}