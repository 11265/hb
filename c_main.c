// c_main.c

#include "内存模块.h"
#include "查找进程.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mach/mach.h>

#define TARGET_PROCESS_NAME "pvz"

int c_main() {
    pid_t target_pid = get_pid_by_name(TARGET_PROCESS_NAME);
    if (target_pid == -1) {
        printf("无法找到目标进程: %s\n", TARGET_PROCESS_NAME);
        return 1;
    }

    printf("找到目标进程 %s, PID: %d\n", TARGET_PROCESS_NAME, target_pid);

    if (初始化内存模块(target_pid) != 0) {
        printf("初始化内存模块失败\n");
        return 1;
    }

    printf("内存模块初始化成功\n");

    // 列出内存区域
    mach_port_t task;
    kern_return_t kr = task_for_pid(mach_task_self(), target_pid, &task);
    if (kr != KERN_SUCCESS) {
        printf("无法获取目标进程的task端口\n");
        关闭内存模块();
        return 1;
    }

    vm_address_t address = 0;
    vm_size_t size;
    uint32_t depth = 0;
    while (1) {
        vm_region_submap_info_data_64_t info;
        mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
        kr = vm_region_recurse_64(task, &address, &size, &depth,
                                  (vm_region_info_t)&info, &count);
        if (kr != KERN_SUCCESS) break;

        printf("Region: 0x%llx - 0x%llx (%llu bytes)\n", address, address + size, size);

        // 尝试读取这个区域的开始部分
        void* data = 读任意地址(address, sizeof(int32_t));
        if (data) {
            printf("  读取成功: 0x%x\n", *(int32_t*)data);
            free(data);

            // 尝试写入
            int32_t test_value = 0x12345678;
            if (写任意地址(address, &test_value, sizeof(int32_t)) == 0) {
                printf("  写入成功\n");
                // 验证写入
                data = 读任意地址(address, sizeof(int32_t));
                if (data && *(int32_t*)data == test_value) {
                    printf("  验证成功\n");
                }
                free(data);
            } else {
                printf("  写入失败\n");
            }
        } else {
            printf("  读取失败\n");
        }

        address += size;
        if (depth == 0) break;
    }

    关闭内存模块();
    printf("内存模块已关闭\n");

    return 0;
}