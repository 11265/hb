#ifndef MEMORY_READ_H
#define MEMORY_READ_H

#include <sys/types.h>
#include <mach/mach_types.h>

int initialize_memory_access(pid_t pid, mach_vm_address_t address, size_t size);
void cleanup_memory_access();

int32_t 读内存i32(mach_vm_address_t address);
int64_t 读内存i64(mach_vm_address_t address);
float   读内存f32(mach_vm_address_t address);
double  读内存f64(mach_vm_address_t address);

#endif