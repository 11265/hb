#include <stdio.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <mach/mach_error.h>

// 全局定义
#define TARGET_PID 22496
#define TARGET_ADDRESS 0x102DD2404

typedef enum {
    TYPE_INT32,
    TYPE_INT64,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_BYTES
} MemoryType;

// 函数声明
int read_typed_memory(pid_t target_pid, vm_address_t target_address, MemoryType type, size_t size, void* result);
int read_int32(pid_t target_pid, vm_address_t target_address, int32_t* result);
int read_int64(pid_t target_pid, vm_address_t target_address, int64_t* result);
int read_float(pid_t target_pid, vm_address_t target_address, float* result);
int read_double(pid_t target_pid, vm_address_t target_address, double* result);
int read_bytes(pid_t target_pid, vm_address_t target_address, size_t size, unsigned char* result);

// 实现 read_typed_memory 函数
int read_typed_memory(pid_t target_pid, vm_address_t target_address, MemoryType type, size_t size, void* result) {
    mach_port_t task;
    kern_return_t kr = task_for_pid(mach_task_self(), target_pid, &task);
    if (kr != KERN_SUCCESS) {
        printf("无法获取目标进程的任务端口。错误码: %d (%s)\n", kr, mach_error_string(kr));
        return 1;
    }

    vm_size_t data_size = size;
    kr = vm_read_overwrite(task, target_address, size, (vm_address_t)result, &data_size);
    if (kr != KERN_SUCCESS) {
        printf("无法读取内存。错误码: %d (%s)\n", kr, mach_error_string(kr));
        mach_port_deallocate(mach_task_self(), task);
        return 1;
    }

    mach_port_deallocate(mach_task_self(), task);
    return 0;
}

// 实现 read_int32 函数
int read_int32(pid_t target_pid, vm_address_t target_address, int32_t* result) {
    return read_typed_memory(target_pid, target_address, TYPE_INT32, sizeof(int32_t), result);
}

// 实现 read_int64 函数
int read_int64(pid_t target_pid, vm_address_t target_address, int64_t* result) {
    return read_typed_memory(target_pid, target_address, TYPE_INT64, sizeof(int64_t), result);
}

// 实现 read_float 函数
int read_float(pid_t target_pid, vm_address_t target_address, float* result) {
    return read_typed_memory(target_pid, target_address, TYPE_FLOAT, sizeof(float), result);
}

// 实现 read_double 函数
int read_double(pid_t target_pid, vm_address_t target_address, double* result) {
    return read_typed_memory(target_pid, target_address, TYPE_DOUBLE, sizeof(double), result);
}

// 实现 read_bytes 函数
int read_bytes(pid_t target_pid, vm_address_t target_address, size_t size, unsigned char* result) {
    return read_typed_memory(target_pid, target_address, TYPE_BYTES, size, result);
}

int c_main() {
    printf("目标进程 PID: %d\n", TARGET_PID);
    printf("目标内存地址: 0x%llx\n", (unsigned long long)TARGET_ADDRESS);
    
    int32_t int32_value;
    int64_t int64_value;
    float float_value;
    double double_value;
    unsigned char bytes[16];  // 假设我们要读取16字节

    if (read_int32(TARGET_PID, TARGET_ADDRESS, &int32_value) == 0) {
        printf("读取的 int32 值: %d\n", int32_value);
    }

    if (read_int64(TARGET_PID, TARGET_ADDRESS, &int64_value) == 0) {
        printf("读取的 int64 值: %lld\n", int64_value);
    }

    if (read_float(TARGET_PID, TARGET_ADDRESS, &float_value) == 0) {
        printf("读取的 float 值: %f\n", float_value);
    }

    if (read_double(TARGET_PID, TARGET_ADDRESS, &double_value) == 0) {
        printf("读取的 double 值: %lf\n", double_value);
    }

    if (read_bytes(TARGET_PID, TARGET_ADDRESS, 16, bytes) == 0) {
        printf("读取的字节: ");
        for (int i = 0; i < 16; i++) {
            printf("%02X ", bytes[i]);
        }
        printf("\n");
    }

    printf("c_main 执行完成\n");
    return 0;
}