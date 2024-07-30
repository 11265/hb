// c_main.c

#include "内存模块.h"
#include "查找进程.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int c_main(void) {
    pid_t target_pid = get_pid_by_name("pvz");
    if (target_pid == -1) {
        printf("未找到目标进程\n");
        return 1;
    }
    printf("找到目标进程 pvz, PID: %d\n", target_pid);

    if (初始化内存模块(target_pid) != 0) {
        printf("内存模块初始化失败\n");
        return 1;
    }
    printf("内存模块初始化成功\n");

    vm_address_t test_address = 0x106BEFE70; // 示例地址，请根据实际情况修改

    // 读取其他数据类型
    printf("读取 int32: %d\n", 读内存i32(test_address));

    // 测试写入和读取 int32_t
    int32_t test_i32 = 12345;
    if (写内存i32(test_address, test_i32) == 0) 
    {
        printf("写入 int32 成功: %d\n", test_i32);
        printf("读取 int32: %d\n", 读内存i32(test_address));
    } else {
        printf("写入 int32 失败\n");
    }


    关闭内存模块();
    return 0;
}