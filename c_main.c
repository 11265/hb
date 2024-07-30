// c_main.c

#include "内存模块.h"
#include "查找进程.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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

    vm_address_t test_address = 0x106befe70; // 示例地址，请根据实际情况修改

    // 读取各种数据类型
    // int32_t
    printf("读取 int32: %d\n", 读内存i32(test_address));

    // int64_t
    printf("读取 int64: %lld\n", 读内存i64(test_address));

    // float
    printf("读取 float: %f\n", 读内存f32(test_address));

    // double
    printf("读取 double: %.14f\n", 读内存f64(test_address));

    // 读取其他类型（使用读任意地址）
    // int8_t
    MemoryReadResult result_i8 = 读任意地址(test_address, sizeof(int8_t));
    if (result_i8.data) {
        printf("读取 int8: %d\n", *(int8_t*)result_i8.data);
        if (result_i8.from_pool) {
            内存池释放(&memory_pool, result_i8.data);
        } else {
            free(result_i8.data);
        }
    }

    // uint32_t
    MemoryReadResult result_u32 = 读任意地址(test_address, sizeof(uint32_t));
    if (result_u32.data) {
        printf("读取 uint32: %u\n", *(uint32_t*)result_u32.data);
        if (result_u32.from_pool) {
            内存池释放(&memory_pool, result_u32.data);
        } else {
            free(result_u32.data);
        }
    }

    // 读取字符串
    size_t string_length = 20; // 假设字符串最大长度为20，根据实际情况调整
    MemoryReadResult result_str = 读任意地址(test_address, string_length);
    if (result_str.data) {
        printf("读取字符串: %s\n", (char*)result_str.data);
        if (result_str.from_pool) {
            内存池释放(&memory_pool, result_str.data);
        } else {
            free(result_str.data);
        }
    }

    关闭内存模块();
    return 0;
}