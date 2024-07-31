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

    // 读取 int32
    int32_t* read_value = (int32_t*)读任意地址(test_address, sizeof(int32_t));
    if (read_value) {
        printf("读取 int32: %d\n", *read_value);
        free(read_value);
    } else {
        printf("读取 int32 失败\n");
    }

    // 测试写入和读取 int32_t
    int32_t test_i32 = 12345;
    if (写任意地址(test_address, &test_i32, sizeof(int32_t)) == 0) {
        printf("写入 int32 成功: %d\n", test_i32);
        
        // 再次读取以验证写入是否成功
        int32_t* verify_value = (int32_t*)读任意地址(test_address, sizeof(int32_t));
        if (verify_value) {
            printf("验证读取 int32: %d\n", *verify_value);
            free(verify_value);
        } else {
            printf("验证读取 int32 失败\n");
        }
    } else {
        printf("写入 int32 失败\n");
    }

    关闭内存模块();
    printf("内存模块已关闭\n");

    return 0;
}