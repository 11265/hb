#ifndef MEMORY_READ_H
#define MEMORY_READ_H

#include <mach/mach.h>
#include <sys/types.h>
#include <sys/mman.h>

int initialize_memory_access(pid_t pid, mach_vm_address_t address, size_t size);
void cleanup_memory_access();

// 读取函数
int32_t 读内存i32(mach_vm_address_t address);
int64_t 读内存i64(mach_vm_address_t address);
float   读内存f32(mach_vm_address_t address);
double  读内存f64(mach_vm_address_t address);

// 写入函数
void 写内存i32(mach_vm_address_t address, int32_t value);
void 写内存i64(mach_vm_address_t address, int64_t value);
void 写内存f32(mach_vm_address_t address, float value);
void 写内存f64(mach_vm_address_t address, double value);

#endif