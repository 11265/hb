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

    // 使用多级指针读取示例
    const char* module_name = "pvz";
    vm_address_t offset = 0x20A88C8;
    int offsets[] = {0x5C8};
    int level = 2;

    vm_address_t final_address = 读模块多级指针(module_name, offset, offsets, level);
    if (final_address == 0) {
        printf("读取多级指针失败\n");
    } else {
        printf("最终地址: 0x%llx\n", (unsigned long long)final_address);

        // 读取最终地址的值
        int32_t value = 读内存i32(final_address);
        printf("读取的值: %d\n", value);
    }

    关闭内存模块();
    printf("关闭内存模块 'pvz'\n");
    return 0;
}