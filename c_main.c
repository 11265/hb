// c_main.c

#include "内存模块.h"
#include "查找进程.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // 为了使用 usleep 函数

#define READS_PER_SECOND 10
#define MICROSECONDS_PER_SECOND 1000000

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

    printf("开始循环读取内存，按 Ctrl+C 退出\n");

    while (1) {
        for (int i = 0; i < READS_PER_SECOND; i++) {
            printf("读取次数: %d\n", i + 1);
            printf("读取 int32: %d\n", 读内存i32(test_address));
            printf("读取 int64: %lld\n", 读内存i64(test_address));
            printf("读取 float: %f\n", 读内存f32(test_address));
            printf("读取 double: %f\n", 读内存f64(test_address));
            printf("\n");

            // 休眠一段时间，以达到每秒10次的频率
            usleep(MICROSECONDS_PER_SECOND / READS_PER_SECOND);
        }
    }

    // 注意：以下代码在无限循环中永远不会执行
    // 您需要通过 Ctrl+C 来终止程序
    关闭内存模块();
    printf("关闭内存模块\n");
    return 0;
}