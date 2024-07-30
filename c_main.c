// c_main.c

#include "内存模块.h"
#include "查找进程.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define TARGET_PROCESS_NAME "pvz"

int c_main(void) {
    pid_t target_pid = get_pid_by_name(TARGET_PROCESS_NAME);
    if (target_pid == -1) {
        fprintf(stderr, "未找到进程：%s\n", TARGET_PROCESS_NAME);
        return -1;
    }
    
    printf("找到目标进程 pvz, PID: %d\n", target_pid);

    if (初始化内存模块(target_pid) != 0) {
        printf("内存模块初始化失败\n");
        return 1;
    }
    printf("内存模块初始化成功\n");

    // 获取 "pvz" 模块的基地址
    mach_vm_address_t base_address = 获取模块基地址("pvz");
    if (base_address == 0) {
        printf("获取模块基地址失败\n");
        关闭内存模块();
        return 1;
    }

    关闭内存模块();
    printf("关闭内存模块 'pvz'\n");
    return 0;
}