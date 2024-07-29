#include <sys/types.h>
#include <mach/mach.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "内存模块.h"

#define TARGET_PID 22496
#define TARGET_ADDRESS 0x102DD2404
#define TARGET_ADDRESS1 0x102dcc678
#define TARGET_ADDRESS2 0x10940c318

int c_main() {
    printf("目标进程 PID: %d\n", TARGET_PID);

    if (initialize_task_port(TARGET_PID) != KERN_SUCCESS) {
        printf("初始化任务端口失败\n");
        return -1;
    }

    // 读取示例
    int32_t int32_value = 读内存i32(TARGET_ADDRESS);
    printf("读取 int32_t 数据: %d\n", int32_value);

/*
    int64_t int64_value = 读内存i64(TARGET_ADDRESS);
    printf("读取 int64_t 数据: %lld\n", int64_value);

    float float_value = 读内存f32(TARGET_ADDRESS);
    printf("读取 float 数据: %f\n", float_value);

    double double_value = 读内存f64(TARGET_ADDRESS);
    printf("读取 double 数据: %f\n", double_value);

    // 写入示例
    写内存i32(TARGET_ADDRESS, 12345);
    printf("写入 int32_t 数据: 12345\n");
    int32_value = 读内存i32(TARGET_ADDRESS);
    printf("读取 int32_t 数据: %d\n", int32_value);
*/
    cleanup_task_port();//释放任务端口

    printf("c_main 执行完成\n");
    return 0;
}