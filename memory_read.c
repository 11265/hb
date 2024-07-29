#include <stdio.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <mach/mach_error.h>


// 新增: 读取32位整数
int read_int32(pid_t target_pid, vm_address_t target_address) {
    return read_typed_memory(target_pid, target_address, TYPE_INT32, sizeof(int32_t));
}

// 新增: 读取64位整数
int read_int64(pid_t target_pid, vm_address_t target_address) {
    return read_typed_memory(target_pid, target_address, TYPE_INT64, sizeof(int64_t));
}

// 新增: 读取浮点数
int read_float(pid_t target_pid, vm_address_t target_address) {
    return read_typed_memory(target_pid, target_address, TYPE_FLOAT, sizeof(float));
}

// 新增: 读取双精度浮点数
int read_double(pid_t target_pid, vm_address_t target_address) {
    return read_typed_memory(target_pid, target_address, TYPE_DOUBLE, sizeof(double));
}

// 新增: 读取指定字节数
int read_bytes(pid_t target_pid, vm_address_t target_address, size_t size) {
    return read_typed_memory(target_pid, target_address, TYPE_BYTES, size);
}