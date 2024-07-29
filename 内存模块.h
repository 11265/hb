#ifndef MEMORY_READ_H
#define MEMORY_READ_H

#include <mach/mach.h>
#include <sys/types.h>

int initialize_task_port(pid_t pid);
void cleanup_task_port();
int32_t 读内存i32(mach_vm_address_t address);
int64_t 读内存i64(mach_vm_address_t address);
float 读内存f32(mach_vm_address_t address);
double 读内存f64(mach_vm_address_t address);

#endif