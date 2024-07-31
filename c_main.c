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

    vm_address_t test_address = 0x1060E1388; // 示例地址，请根据实际情况修改

    // 测试读取各种数据类型
    printf("读取 int32: %d\n", 读内存i32(test_address));
    printf("读取 int64: %lld\n", 读内存i64(test_address));
    printf("读取 float: %f\n", 读内存f32(test_address));
    printf("读取 double: %f\n", 读内存f64(test_address));

    // 测试读取字符串
    MemoryReadResult result = 读任意地址(test_address, 20); // 假设读取20字节的字符串
    if (result.data) {
        printf("读取字符串: %s\n", (char*)result.data);
        if (result.from_pool) {
            内存池释放(&memory_pool, result.data);
        } else {
            free(result.data);
        }
    }

    关闭内存模块();
    printf("关闭内存模块\n");
    return 0;
}