#ifndef MEMORY_UTILS_H
#define MEMORY_UTILS_H

#include <stdint.h>
#include <mach/mach.h>

// 添加 "读内存" 函数的声明
int 读内存(vm_address_t address, void* buffer, size_t size);

// 查找模块基地址的函数
vm_address_t find_module_base(task_t target_task, const char* module_name);

// 多层指针读取函数
int64_t read_multi_level_pointer(task_t target_task, vm_address_t base_address, int num_offsets, ...);

#endif // MEMORY_UTILS_H