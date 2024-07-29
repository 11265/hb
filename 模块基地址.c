#include "模块基地址.h"
#include "内存模块.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <mach-o/dyld_images.h>

vm_address_t find_module_base(task_t target_task, const char* module_name) {
    task_dyld_info_data_t task_dyld_info;
    mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
    if (task_info(target_task, TASK_DYLD_INFO, (task_info_t)&task_dyld_info, &count) != KERN_SUCCESS) {
        printf("获取 task_info 失败\n");
        return 0;
    }

    struct dyld_all_image_infos* all_image_infos = (struct dyld_all_image_infos*)task_dyld_info.all_image_info_addr;
    struct dyld_all_image_infos infos;
    memcpy(&infos, (void*)异步读内存i64((vm_address_t)all_image_infos), sizeof(infos));

    for (uint32_t i = 0; i < infos.infoArrayCount; i++) {
        struct dyld_image_info image_info;
        memcpy(&image_info, (void*)异步读内存i64((vm_address_t)&infos.infoArray[i]), sizeof(image_info));

        char path[1024];
        vm_address_t path_address = (vm_address_t)image_info.imageFilePath;
        for (int j = 0; j < sizeof(path); j += 8) {
            int64_t chunk = 异步读内存i64(path_address + j);
            memcpy(path + j, &chunk, (j + 8 <= sizeof(path)) ? 8 : sizeof(path) - j);
        }
        path[sizeof(path) - 1] = '\0';

        if (strstr(path, module_name) != NULL) {
            return (vm_address_t)image_info.imageLoadAddress;
        }
    }

    return 0;
}

int64_t read_multi_level_pointer(task_t target_task, vm_address_t base_address, int num_offsets, ...) {
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