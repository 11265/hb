#include <sys/types.h>
#include <mach/mach.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "内存模块.h"
#include <sys/types.h>  // Add this line if not already present

#define TARGET_PID 22496
#define TARGET_ADDRESS 0x102DD2404

int c_main() {
    printf("目标进程 PID: %d\n", TARGET_PID);
    printf("目标内存地址: 0x%llx\n", (unsigned long long)TARGET_ADDRESS);

    if (initialize_task_port(TARGET_PID) != KERN_SUCCESS) {
        printf("初始化任务端口失败\n");
        return -1;
    }

    int32_t int32_value;
    if (read_int32(TARGET_ADDRESS, &int32_value) == 0) {
        printf("读取 int32_t 数据成功: %d\n", int32_value);
    } else {
        printf("读取 int32_t 数据失败\n");
    }

    int64_t int64_value;
    if (read_int64(TARGET_ADDRESS, &int64_value) == 0) {
        printf("读取 int64_t 数据成功: %lld\n", int64_value);
    } else {
        printf("读取 int64_t 数据失败\n");
    }

    cleanup_task_port();

    printf("c_main 执行完成\n");
    return 0;
}