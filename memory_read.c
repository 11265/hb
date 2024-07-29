#include <stdio.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <mach/mach_error.h>

// 添加类型定义
typedef enum {
    TYPE_INT32,
    TYPE_INT64,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_BYTES
} MemoryType;

// 声明 read_typed_memory 函数
int read_typed_memory(pid_t target_pid, vm_address_t target_address, MemoryType type, size_t size);

// 实现 read_typed_memory 函数
int read_typed_memory(pid_t target_pid, vm_address_t target_address, MemoryType type, size_t size) {
    mach_port_t task;
    kern_return_t kr = task_for_pid(mach_task_self(), target_pid, &task);
    if (kr != KERN_SUCCESS) {
        printf("无法获取目标进程的任务端口。错误码: %d (%s)\n", kr, mach_error_string(kr));
        return 1;
    }

    void *data = malloc(size);
    mach_msg_type_number_t data_size = size;
    kr = vm_read_overwrite(task, target_address, size, (vm_address_t)data, &data_size);
    if (kr != KERN_SUCCESS) {
        printf("无法读取内存。错误码: %d (%s)\n", kr, mach_error_string(kr));
        mach_port_deallocate(mach_task_self(), task);
        free(data);
        return 1;
    }

    switch (type) {
        case TYPE_INT32:
            printf("读取的 int32 值: %d\n", *(int32_t*)data);
            break;
        case TYPE_INT64:
            printf("读取的 int64 值: %lld\n", *(int64_t*)data);
            break;
        case TYPE_FLOAT:
            printf("读取的 float 值: %f\n", *(float*)data);
            break;
        case TYPE_DOUBLE:
            printf("读取的 double 值: %lf\n", *(double*)data);
            break;
        case TYPE_BYTES:
            printf("读取的字节: ");
            for (size_t i = 0; i < size; i++) {
                printf("%02X ", ((unsigned char*)data)[i]);
            }
            printf("\n");
            break;
    }

    mach_port_deallocate(mach_task_self(), task);
    free(data);
    return 0;
}

// 读取32位整数
int read_int32(pid_t target_pid, vm_address_t target_address) {
    return read_typed_memory(target_pid, target_address, TYPE_INT32, sizeof(int32_t));
}

// 读取64位整数
int read_int64(pid_t target_pid, vm_address_t target_address) {
    return read_typed_memory(target_pid, target_address, TYPE_INT64, sizeof(int64_t));
}

// 读取浮点数
int read_float(pid_t target_pid, vm_address_t target_address) {
    return read_typed_memory(target_pid, target_address, TYPE_FLOAT, sizeof(float));
}

// 读取双精度浮点数
int read_double(pid_t target_pid, vm_address_t target_address) {
    return read_typed_memory(target_pid, target_address, TYPE_DOUBLE, sizeof(double));
}

// 读取指定字节数
int read_bytes(pid_t target_pid, vm_address_t target_address, size_t size) {
    return read_typed_memory(target_pid, target_address, TYPE_BYTES, size);
}