#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>

#define TARGET_PID 23569
#define BASE_ADDRESS 0x00000000
#define MAP_SIZE 0x200000000

int c_main(void) {
    printf("开始初始化内存访问...\n");
    int result = initialize_memory_access(TARGET_PID, BASE_ADDRESS, MAP_SIZE);
    if (result != 0) {
        fprintf(stderr, "无法初始化内存访问，错误代码：%d\n", result);
        return -1;
    }
    printf("内存访问初始化成功\n");

    vm_address_t target_address = 0x104BB7C78;
    printf("尝试读取地址 0x%llx\n", (unsigned long long)target_address);
    
    int32_t int_value = 读内存i32(target_address);
    printf("地址 0x%llx 处读取的 int32_t 值: %d (0x%x)\n", 
           (unsigned long long)target_address, int_value, int_value);

    cleanup_memory_access();
    printf("清理完成\n");
    return 0;
}