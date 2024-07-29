// 写入示例
//写内存f64(TARGET_ADDRESS, 2.71828182845904);
//printf("写入 double 数据: 2.71828182845904\n");
//double_value = 读内存f64(TARGET_ADDRESS);
//printf("读取 double 数据: %f\n", double_value);

// 读取示例
//int32_t int32_value = 读内存i32(TARGET_ADDRESS);
//printf("读取 int32_t 数据: %d\n", int32_value);

#ifndef MEMORY_READ_H
#define MEMORY_READ_H

#include <mach/mach.h>
#include <sys/types.h>

int initialize_task_port(pid_t pid);
void cleanup_task_port();

// 读取函数
int32_t 读内存i32(mach_vm_address_t address);
int64_t 读内存i64(mach_vm_address_t address);
float 	读内存f32(mach_vm_address_t address);
double 	读内存f64(mach_vm_address_t address);

// 写入函数
void 写内存i32(mach_vm_address_t address, int32_t value);
void 写内存i64(mach_vm_address_t address, int64_t value);
void 写内存f32(mach_vm_address_t address, float value);
void 写内存f64(mach_vm_address_t address, double value);

#endif