#ifndef MEMORY_READ_H
#define MEMORY_READ_H

#include <mach/mach.h>

int read_int32(mach_vm_address_t address, int32_t *value);
int read_int64(mach_vm_address_t address, int64_t *value);

#endif // MEMORY_READ_H