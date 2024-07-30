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

    关闭内存模块();
    return 0;
}