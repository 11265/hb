// 写入示例
//写内存i32(0x10940c318, 12345);

#ifndef MEMORY_READ_H
#define MEMORY_READ_H

#include <mach/mach.h>
#include <sys/types.h>

int initialize_task_port(pid_t pid);
void cleanup_task_port();

// 读取函数
int32_t 读内存i32(mach_vm_address_t address);
int64_t 读内存i64(mach_vm_address_t address);
float 读内存f32(mach_vm_address_t address);
double 读内存f64(mach_vm_address_t address);

// 写入函数
void 写内存i32(mach_vm_address_t address, int32_t value);
void 写内存i64(mach_vm_address_t address, int64_t value);
void 写内存f32(mach_vm_address_t address, float value);
void 写内存f64(mach_vm_address_t address, double value);

#endif