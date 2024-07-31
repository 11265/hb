#include "内存模块.h"
#include "查找进程.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

    uintptr_t test_address = 0x1060E1388; // 示例地址，请根据实际情况修改
    size_t map_size = 4096; // 映射大小，可以根据需要调整

    MappedMemory* mapped_memory = 创建内存映射(test_address, map_size);
    if (!mapped_memory) {
        printf("创建内存映射失败\n");
        关闭内存模块();
        return 1;
    }

    printf("开始循环读取内存，按 Ctrl+C 退出\n");

    while (1) {
        for (int i = 0; i < READS_PER_SECOND; i++) {
            printf("读取次数: %d\n", i + 1);
            printf("读取 int32: %d\n", 读内存i32(mapped_memory, 0));
            // 如果需要读取其他类型，可以取消下面的注释
            // printf("读取 int64: %lld\n", 读内存i64(mapped_memory, 0));
            // printf("读取 float: %f\n", 读内存f32(mapped_memory, 0));
            // printf("读取 double: %f\n", 读内存f64(mapped_memory, 0));

            usleep(MICROSECONDS_PER_SECOND / READS_PER_SECOND);
        }
    }


    // 注意：以下代码在无限循环中永远不会执行
    // 您需要通过 Ctrl+C 来终止程序
    释放内存映射(mapped_memory);
    关闭内存模块();
    printf("关闭内存模块\n");
    return 0;
}