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

        if (initialize_task_port(TARGET_PID) != KERN_SUCCESS) {
        printf("初始化任务端口失败\n");
        return -1;
    }

    printf("目标进程 PID: %d\n", TARGET_PID);
    printf("目标内存地址: 0x%llx\n", (unsigned long long)TARGET_ADDRESS);



    int32_t int32_value = 读内存i32(TARGET_ADDRESS);
    printf("读取 int32_t 数据: %d\n", int32_value);

    int64_t int64_value = 读内存i64(TARGET_ADDRESS1);
    printf("读取 int64_t 数据: %lld\n", int64_value);

    float float_value = 读内存f32(TARGET_ADDRESS);
    printf("读取 float 数据: %f\n", float_value);

    double double_value = 读内存f64(TARGET_ADDRESS2);
    printf("读取 double 数据: %f\n", double_value);

    cleanup_task_port();

    printf("c_main 执行完成\n");
    return 0;
}