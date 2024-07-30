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

    vm_address_t test_address = 0x12345000; // 示例地址，请根据实际情况修改

    // 测试写入和读取各种数据类型
    int32_t test_i32 = 42;
    写内存i32(test_address, test_i32);
    printf("写入 int32: %d, 读取 int32: %d\n", test_i32, 读内存i32(test_address));

    int64_t test_i64 = 1234567890123LL;
    写内存i64(test_address, test_i64);
    printf("写入 int64: %lld, 读取 int64: %lld\n", test_i64, 读内存i64(test_address));

    float test_f32 = 3.14159f;
    写内存f32(test_address, test_f32);
    printf("写入 float: %f, 读取 float: %f\n", test_f32, 读内存f32(test_address));

    double test_f64 = 2.71828182845904;
    写内存f64(test_address, test_f64);
    printf("写入 double: %f, 读取 double: %f\n", test_f64, 读内存f64(test_address));

    // 测试写入和读取字符串
    const char* test_string = "Hello, World!";
    写任意地址(test_address, test_string, strlen(test_string) + 1);
    printf("写入字符串: %s\n", test_string);

    MemoryReadResult result = 读任意地址(test_address, strlen(test_string) + 1);
    if (result.data) {
        printf("读取字符串: %s\n", (char*)result.data);
        if (result.from_pool) {
            内存池释放(&memory_pool, result.data);
        } else {
            free(result.data);
        }
    }

    关闭内存模块();
    return 0;
}