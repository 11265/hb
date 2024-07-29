#include <stdio.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <mach/mach_error.h>

// 定义支持的数据类型
typedef enum {
    TYPE_INT32,
    TYPE_INT64,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_BYTES
} DataType;

int read_process_memory() {
    pid_t target_pid;
    vm_address_t target_address;
    vm_size_t size_to_read = 4;  // 减小到4字节

    printf("输入目标进程的 PID: ");
    scanf("%d", &target_pid);

    printf("输入要读取的内存地址（十六进制）: ");
    scanf("%llx", &target_address);

    // 获取目标进程的任务端口
    mach_port_t task;
    kern_return_t kr = task_for_pid(mach_task_self(), target_pid, &task);
    if (kr != KERN_SUCCESS) {
        printf("无法获取目标进程的任务端口。错误码: %d (%s)\n", kr, mach_error_string(kr));
        return 1;
    }

    // 读取内存
    vm_offset_t data[size_to_read / sizeof(vm_offset_t) + 1];
    mach_msg_type_number_t data_size = size_to_read;
    kr = vm_read_overwrite(task, target_address, size_to_read, (vm_address_t)data, &data_size);
    if (kr != KERN_SUCCESS) {
        printf("无法读取内存。错误码: %d (%s)\n", kr, mach_error_string(kr));
        mach_port_deallocate(mach_task_self(), task);
        return 1;
    }

    // 打印读取的数据
    printf("读取的数据:\n");
    unsigned char *buffer = (unsigned char *)data;
    for (int i = 0; i < data_size; i++) {
        printf("%02X ", buffer[i]);
    }
    printf("\n");

    // 释放资源
    mach_port_deallocate(mach_task_self(), task);

    return 0;
}

// 新增: 读取指定类型的内存数据
int read_typed_memory(pid_t target_pid, vm_address_t target_address, DataType type, size_t size) {
    mach_port_t task;
    kern_return_t kr = task_for_pid(mach_task_self(), target_pid, &task);
    if (kr != KERN_SUCCESS) {
        printf("无法获取目标进程的任务端口。错误码: %d (%s)\n", kr, mach_error_string(kr));
        return 1;
    }

    vm_offset_t data[size / sizeof(vm_offset_t) + 1];
    mach_msg_type_number_t data_size = size;
    kr = vm_read_overwrite(task, target_address, size, (vm_address_t)data, &data_size);
    if (kr != KERN_SUCCESS) {
        printf("无法读取内存。错误码: %d (%s)\n", kr, mach_error_string(kr));
        mach_port_deallocate(mach_task_self(), task);
        return 1;
    }

    printf("读取的数据: ");
    switch (type) {
        case TYPE_INT32:
            printf("%d\n", *(int32_t*)data);
            break;
        case TYPE_INT64:
            printf("%lld\n", *(int64_t*)data);
            break;
        case TYPE_FLOAT:
            printf("%f\n", *(float*)data);
            break;
        case TYPE_DOUBLE:
            printf("%lf\n", *(double*)data);
            break;
        case TYPE_BYTES:
            for (int i = 0; i < data_size; i++) {
                printf("%02X ", ((unsigned char*)data)[i]);
            }
            printf("\n");
            break;
    }

    mach_port_deallocate(mach_task_self(), task);
    return 0;
}

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