#include "memory_read.h"
#include <stdio.h>
#include <stdlib.h>
#include <mach/mach_error.h>

// 辅助函数，用于读取内存
static int read_memory(HANDLE process_handle, DWORD address, void* result, size_t size) {
    vm_size_t data_size = size;
    kern_return_t kr = vm_read_overwrite(process_handle, (vm_address_t)address, size, (vm_address_t)result, &data_size);
    if (kr != KERN_SUCCESS) {
        printf("无法读取内存。错误码: %d (%s)\n", kr, mach_error_string(kr));
        return 0;
    }
    return 1;
}

DWORD read_int32(HANDLE process_handle, DWORD address) {
    int32_t result = 0;
    if (read_memory(process_handle, address, &result, sizeof(result))) {
        return (DWORD)result;
    }
    return 0;
}

long long read_int64(HANDLE process_handle, DWORD address) {
    int64_t result = 0;
    if (read_memory(process_handle, address, &result, sizeof(result))) {
        return result;
    }
    return 0;
}

float read_float(HANDLE process_handle, DWORD address) {
    float result = 0;
    if (read_memory(process_handle, address, &result, sizeof(result))) {
        return result;
    }
    return 0;
}

double read_double(HANDLE process_handle, DWORD address) {
    double result = 0;
    if (read_memory(process_handle, address, &result, sizeof(result))) {
        return result;
    }
    return 0;
}

int read_bytes(HANDLE process_handle, DWORD address, size_t size, unsigned char* result) {
    return read_memory(process_handle, address, result, size);
}