#ifndef MEMORY_MODULE_H
#define MEMORY_MODULE_H

#include <mach/mach.h>
#include <stdint.h>

int initialize_memory_access(pid_t pid, vm_address_t address, vm_size_t size);
void cleanup_memory_access(void);
int32_t 读内存i32(vm_address_t address);

#endif // MEMORY_MODULE_H