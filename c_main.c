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

    int result = 初始化内存模块(target_pid);
    if (result != 0) {
        fprintf(stderr, "无法初始化内存模块，错误代码：%d\n", result);
        return -1;
    }
    printf("内存模块初始化成功\n");

    vm_address_t test_address = 0x106befe70; // 示例地址，请根据实际情况修改

    // int32_t
    int32_t test_i32 = 3546;
    写内存i32(test_address, test_i32);
    printf("写入 int32: %d, 读取 int32: %d\n", test_i32, 读内存i32(test_address));

    // int64_t
    int64_t test_i64 = 1234567890123456789LL;
    写内存i64(test_address, test_i64);
    printf("写入 int64: %lld, 读取 int64: %lld\n", test_i64, 读内存i64(test_address));

    // float
    float test_f32 = 3.14159f;
    写内存f32(test_address, test_f32);
    printf("写入 float: %f, 读取 float: %f\n", test_f32, 读内存f32(test_address));

    // double
    double test_f64 = 2.71828182845904523536;
    写内存f64(test_address, test_f64);
    printf("写入 double: %lf, 读取 double: %lf\n", test_f64, 读内存f64(test_address));

    // 任意地址读写测试
    char test_string[] = "Hello, World!";
    size_t string_length = sizeof(test_string);
    写任意地址(test_address, test_string, string_length);
    
    MemoryReadResult result = 读任意地址(test_address, string_length);
    if (result.data) {
        printf("写入字符串: %s, 读取字符串: %s\n", test_string, (char*)result.data);
        if (result.from_pool) {
            内存池释放(&memory_pool, result.data);
        } else {
            free(result.data);
        }
    } else {
        fprintf(stderr, "读取字符串失败\n");
    }

    关闭内存模块();
    printf("关闭内存模块\n");
    return 0;
}