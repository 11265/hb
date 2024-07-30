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

    // 测试 int32
    int32_t write_value_i32 = 42;
    if (写内存i32(test_address, write_value_i32) == 0) {
        int32_t read_value_i32 = 读内存i32(test_address);
        printf("写入 int32: %d, 读取 int32: %d\n", write_value_i32, read_value_i32);
    } else {
        printf("写入 int32 失败\n");
    }

    // 测试 int64
    int64_t write_value_i64 = 1234567890123LL;
    if (写内存i64(test_address, write_value_i64) == 0) {
        int64_t read_value_i64 = 读内存i64(test_address);
        printf("写入 int64: %lld, 读取 int64: %lld\n", write_value_i64, read_value_i64);
    } else {
        printf("写入 int64 失败\n");
    }

    // 测试 float
    float write_value_f32 = 3.14159f;
    if (写内存f32(test_address, write_value_f32) == 0) {
        float read_value_f32 = 读内存f32(test_address);
        printf("写入 float: %f, 读取 float: %f\n", write_value_f32, read_value_f32);
    } else {
        printf("写入 float 失败\n");
    }

    // 测试 double
    double write_value_f64 = 2.71828182845904523536;
    if (写内存f64(test_address, write_value_f64) == 0) {
        double read_value_f64 = 读内存f64(test_address);
        printf("写入 double: %lf, 读取 double: %lf\n", write_value_f64, read_value_f64);
    } else {
        printf("写入 double 失败\n");
    }

    // 测试读写任意地址
    const char* test_string = "Hello, World!";
    size_t test_string_length = strlen(test_string) + 1; // 包括空字符

    if (写任意地址(0x200000000, test_string, test_string_length) == 0) {
        char* read_string = (char*)读任意地址(0x200000000, test_string_length);
        if (read_string) {
            printf("写入字符串: %s\n读取字符串: %s\n", test_string, read_string);
            内存释放(read_string);  // 使用内存池的释放函数
        } else {
            printf("读取字符串失败\n");
        }
    } else {
        printf("写入字符串失败\n");
    }

    关闭内存模块();
    printf("内存模块已关闭\n");

    return 0;
}