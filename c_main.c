#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>
#include <mach/mach.h>

#define TARGET_PID 23569
#define TARGET_ADDRESS 0x104bb7c78

int c_main(void) {
    kern_return_t kr;
    task_t target_task;
    vm_address_t base_address;
    vm_size_t region_size;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t info_count;
    mach_port_t object_name;

    printf("开始初始化内存访问...\n");

    // 获取目标进程的 task
    kr = task_for_pid(mach_task_self(), TARGET_PID, &target_task);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "无法获取目标进程的 task: %s\n", mach_error_string(kr));
        return -1;
    }

    // 获取目标地址所在的内存区域信息
    base_address = TARGET_ADDRESS;
    region_size = 0;
    info_count = VM_REGION_BASIC_INFO_COUNT_64;
    kr = vm_region_64(target_task, &base_address, &region_size, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info, &info_count, &object_name);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "无法获取内存区域信息: %s\n", mach_error_string(kr));
        return -1;
    }

    printf("目标地址所在内存区域: 0x%llx - 0x%llx\n", (unsigned long long)base_address, (unsigned long long)(base_address + region_size));

    if (TARGET_ADDRESS < base_address || TARGET_ADDRESS >= base_address + region_size) {
        fprintf(stderr, "目标地址 0x%llx 不在有效的内存区域内\n", (unsigned long long)TARGET_ADDRESS);
        return -1;
    }

    int result = initialize_memory_access(TARGET_PID, base_address, region_size);
    if (result != 0) {
        fprintf(stderr, "无法初始化内存访问，错误代码：%d\n", result);
        return -1;
    }
    printf("内存访问初始化成功\n");

    printf("尝试读取地址 0x%llx\n", (unsigned long long)TARGET_ADDRESS);
    
    int32_t int_value = 读内存i32(TARGET_ADDRESS);
    printf("地址 0x%llx 处读取的 int32_t 值: %d (0x%x)\n", 
           (unsigned long long)TARGET_ADDRESS, int_value, int_value);

    // 尝试读取周围的内存
    for (int i = -2; i <= 2; i++) {
        vm_address_t addr = TARGET_ADDRESS + i * 4;
        int32_t value = 读内存i32(addr);
        printf("地址 0x%llx 处的值: %d (0x%x)\n", (unsigned long long)addr, value, value);
    }

    cleanup_memory_access();
    printf("清理完成\n");
    return 0;
}