#include "内存模块.h"
#include "查找进程.h"
#include <stdio.h>
#include <stdint.h>
#include <mach-o/dyld_images.h>

#define TARGET_PROCESS_NAME "pvz"

// 新增：查找模块基地址的函数
vm_address_t find_module_base(const char* module_name) {
    task_dyld_info_data_t task_dyld_info;
    mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
    if (task_info(target_task, TASK_DYLD_INFO, (task_info_t)&task_dyld_info, &count) != KERN_SUCCESS) {
        printf("获取 task_info 失败\n");
        return 0;
    }

    struct dyld_all_image_infos* all_image_infos = (struct dyld_all_image_infos*)task_dyld_info.all_image_info_addr;
    struct dyld_all_image_infos infos;
    if (读内存((vm_address_t)all_image_infos, &infos, sizeof(infos)) != 0) {
        printf("读取 all_image_infos 失败\n");
        return 0;
    }

    for (uint32_t i = 0; i < infos.infoArrayCount; i++) {
        struct dyld_image_info image_info;
        if (读内存((vm_address_t)&infos.infoArray[i], &image_info, sizeof(image_info)) != 0) {
            continue;
        }

        char path[1024];
        if (读内存((vm_address_t)image_info.imageFilePath, path, sizeof(path)) != 0) {
            continue;
        }

        if (strstr(path, module_name) != NULL) {
            return (vm_address_t)image_info.imageLoadAddress;
        }
    }

    return 0;
}
// 新增：多层指针读取函数
int64_t read_multi_level_pointer(vm_address_t base_address, int num_offsets, ...) {
    va_list args;
    va_start(args, num_offsets);

    int64_t current_address = base_address;
    for (int i = 0; i < num_offsets; i++) {
        int64_t offset = va_arg(args, int64_t);
        current_address = 异步读内存i64(current_address);
        if (current_address == 0) {
            printf("无效的地址在第 %d 层指针\n", i + 1);
            va_end(args);
            return 0;
        }
        current_address += offset;
    }

    va_end(args);
    return current_address;
}

int c_main(void) {
    pid_t target_pid = get_pid_by_name(TARGET_PROCESS_NAME);
    if (target_pid == -1) {
        fprintf(stderr, "未找到进程：%s\n", TARGET_PROCESS_NAME);
        return -1;
    }
    
    printf("找到进程 %s，PID: %d\n", TARGET_PROCESS_NAME, target_pid);

    int result = initialize_memory_module(target_pid);
    if (result != 0) {
        fprintf(stderr, "无法初始化内存模块，错误代码：%d\n", result);
        return -1;
    }
    printf("内存模块初始化成功\n");

    // 查找 "pvz iOS" 模块的基地址
    vm_address_t base_address = find_module_base("pvz");
    if (base_address == 0) {
        fprintf(stderr, "未找到 pvz iOS 模块\n");
        cleanup_memory_module();
        return -1;
    }
    printf("pvz iOS 模块基地址: 0x%llx\n", (unsigned long long)base_address);

    // 读取多层指针
    int64_t final_address = read_multi_level_pointer(base_address, 2, 
                                                     0x20A7AA0, 0x400);
    
    if (final_address != 0) {
        int32_t value = 异步读内存i32(final_address);
        printf("多层指针读取结果: 0x%llx, 值: %d (0x%x)\n", 
               (unsigned long long)final_address, value, value);
    }

    cleanup_memory_module();
    printf("清理完成\n");
    return 0;
}