// c_main.c

#include "内存模块.h"
#include "查找进程.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TARGET_PROCESS_NAME "pvz"

int c_main() {
    pid_t target_pid = get_pid_by_name(TARGET_PROCESS_NAME);
    if (target_pid == -1) {
        printf("无法找到目标进程: %s\n", TARGET_PROCESS_NAME);
        return 1;
    }

    printf("找到目标进程 %s, PID: %d\n", TARGET_PROCESS_NAME, target_pid);

    if (初始化内存模块(target_pid) != 0) {
        printf("初始化内存模块失败\n");
        return 1;
    }

    printf("内存模块初始化成功\n");

    // 测试读写不同类型的内存
    vm_address_t test_address = 0x1060E1388; // 假设这是一个有效的内存地址

    // 读取其他数据类型
    printf("读取 int32: %d\n", 读内存i32(test_address));

    // 测试写入和读取 int32_t
    int32_t test_i32 = 12345;
    if (写内存i32(test_address, test_i32) == 0) {
        printf("写入 int32 成功: %d\n", test_i32);
        printf("读取 int32: %d\n", 读内存i32(test_address));
    } else {
        printf("写入 int32 失败\n");
    }


    关闭内存模块();
    printf("内存模块已关闭\n");

    return 0;
}