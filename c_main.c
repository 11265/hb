// c_main.c

#include "内存模块.h"
#include "查找进程.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define TARGET_PROCESS_NAME "pvz"

int c_main(void) {
    pid_t target_pid = get_pid_by_name(TARGET_PROCESS_NAME);
    if (target_pid == -1) {
        fprintf(stderr, "未找到进程：%s\n", TARGET_PROCESS_NAME);
        return -1;
    }
    
    printf("找到进程 %s，PID: %d\n", TARGET_PROCESS_NAME, target_pid);

    int init_result = 初始化内存模块(target_pid);
    if (init_result != 0) {
        fprintf(stderr, "无法初始化内存模块，错误代码：%d\n", init_result);
        return -1;
    }
    printf("内存模块初始化成功\n");

    vm_address_t test_address = 0x106befe70; // 示例地址，请根据实际情况修改

    // int32_t 测试
    int32_t test_i32 = 3546;
    int write_result = 写内存i32(test_address, test_i32);
    if (write_result != 0) {
        fprintf(stderr, "写入 int32 失败，错误代码：%d\n", write_result);
    } else {
        int32_t read_value = 读内存i32(test_address);
        if (read_value == 0) {
            fprintf(stderr, "读取 int32 失败或读取值为0\n");
        } else {
            printf("写入 int32: %d, 读取 int32: %d\n", test_i32, read_value);
        }
    }

    // int64_t 测试
    int64_t test_i64 = 1234567890123456789LL;
    write_result = 写内存i64(test_address, test_i64);
    if (write_result != 0) {
        fprintf(stderr, "写入 int64 失败，错误代码：%d\n", write_result);
    } else {
        int64_t read_value = 读内存i64(test_address);
        if (read_value == 0) {
            fprintf(stderr, "读取 int64 失败或读取值为0\n");
        } else {
            printf("写入 int64: %lld, 读取 int64: %lld\n", test_i64, read_value);
        }
    }

    关闭内存模块();
    printf("关闭内存模块\n");
    return 0;
}