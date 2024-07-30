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

    vm_address_t test_address = 0x102F83E68; // 示例地址，请根据实际情况修改

    // 检查内存保护
    vm_prot_t protection;
    boolean_t shared;
    vm_size_t size = sizeof(int32_t);
    kern_return_t kr = vm_region(target_task, &test_address, &size, VM_REGION_BASIC_INFO, (vm_region_info_t)&protection, &shared);
    if (kr != KERN_SUCCESS) {
        printf("无法获取内存区域信息: %s\n", mach_error_string(kr));
        关闭内存模块();
        return 1;
    }
    printf("内存保护: %d\n", protection);

    // 读取初始 int32 值
    int32_t initial_value = 读内存i32(test_address);
    printf("初始 int32 值: %d\n", initial_value);

    // 写入新的 int32 值
    int32_t new_value = 42; // 您可以修改这个值
    int write_result = 写内存i32(test_address, new_value);
    if (write_result == 0) {
        printf("成功写入新的 int32 值: %d\n", new_value);
    } else {
        printf("写入新的 int32 值失败, 错误码: %d\n", write_result);
    }

    // 再次读取 int32 值
    int32_t final_value = 读内存i32(test_address);
    printf("最终 int32 值: %d\n", final_value);

    // 验证写入是否成功
    if (final_value == new_value) {
        printf("写入成功验证：新值已正确写入并读取\n");
    } else {
        printf("写入失败验证：读取的值与写入的值不匹配\n");
    }

    关闭内存模块();
    printf("关闭内存模块\n");
    return 0;
}