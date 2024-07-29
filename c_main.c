#include <sys/types.h>                                          // 系统类型定义
#include <mach/mach.h>                                          // Mach相关定义
#include <stdio.h>                                              // 标准输入输出
#include <stdbool.h>                                            // 布尔类型支持
#include <string.h>                                             // 字符串操作
#include "内存模块.h"                                           // 自定义内存模块
#include <sys/types.h>                                          // 系统类型定义（重复包含，可以删除）

#define TARGET_PID 22496                                        // 定义目标进程ID
#define TARGET_ADDRESS 0x102DD2404                              // 定义目标内存地址

int c_main() {
    printf("目标进程 PID: %d\n", TARGET_PID);                   // 打印目标进程ID
    printf("目标内存地址: 0x%llx\n", (unsigned long long)TARGET_ADDRESS);  // 打印目标内存地址

    初始化()

    int32_t int32_value;                                        // 32位整数变量
    if (读内存i32(TARGET_ADDRESS, &int32_value) == 0) {        // 读取32位整数
        printf("读取 int32_t 数据成功: %d\n", int32_value);     // 打印读取的32位整数
    } else {
        printf("读取 int32_t 数据失败\n");                      // 如果读取失败，打印错误信息
    }

    int64_t int64_value;                                        // 64位整数变量
    if (读内存i64(TARGET_ADDRESS, &int64_value) == 0) {        // 读取64位整数
        printf("读取 int64_t 数据成功: %lld\n", int64_value);   // 打印读取的64位整数
    } else {
        printf("读取 int64_t 数据失败\n");                      // 如果读取失败，打印错误信息
    }

    cleanup_task_port();                                        // 清理任务端口

    printf("c_main 执行完成\n");                                // 打印执行完成信息
    return 0;                                                   // 返回成功
}

int 初始化() {

        if (initialize_task_port(TARGET_PID) != KERN_SUCCESS) {     // 初始化任务端口
        printf("初始化任务端口失败\n");                         // 如果初始化失败，打印错误信息
        return -1;                                              // 返回错误
    }
}