#include <stdio.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <mach/mach_error.h>

// 全局定义
#define TARGET_PID 22496
#define TARGET_ADDRESS 0x102DD2404
#define MEMORY_TYPE TYPE_INT32

typedef enum {
    TYPE_INT32,
    TYPE_INT64,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_BYTES
} MemoryType;

// 函数声明
int read_typed_memory(pid_t target_pid, vm_address_t target_address, MemoryType type, size_t size);

// 实现 read_typed_memory 函数
int read_typed_memory(pid_t target_pid, vm_address_t target_address, MemoryType type, size_t size) {
    printf("开始读取内存...\n");
    mach_port_t task;
    kern_return_t kr = task_for_pid(mach_task_self(), target_pid, &task);
    if (kr != KERN_SUCCESS) {
        printf("无法获取目标进程的任务端口。错误码: %d (%s)\n", kr, mach_error_string(kr));
        return 1;
    }
    printf("成功获取任务端口\n");

    void *data = malloc(size);
    if (data == NULL) {
        printf("内存分配失败\n");
        return 1;
    }
    printf("成功分配内存\n");

    mach_msg_type_number_t data_size = size;
    kr = vm_read_overwrite(task, target_address, size, (vm_address_t)data, &data_size);
    if (kr != KERN_SUCCESS) {
        printf("无法读取内存。错误码: %d (%s)\n", kr, mach_error_string(kr));
        mach_port_deallocate(mach_task_self(), task);
        free(data);
        return 1;
    }
    printf("成功读取内存\n");

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
    printf("内存读取完成\n");
    return 0;
}

// 其他函数保持不变...

int c_main() {
    //printf("开始执行 c_main\n");
    printf("目标进程 PID: %d\n", TARGET_PID);
    printf("目标内存地址: 0x%llx\n", (unsigned long long)TARGET_ADDRESS);
    
    int result;
    switch (MEMORY_TYPE) {
        case TYPE_INT32:
            printf("准备读取 INT32\n");
            result = read_int32(TARGET_PID, TARGET_ADDRESS);
            break;
        // 其他 case 保持不变...
        default:
            printf("未知的内存类型\n");
            return 1;
    }

    if (result != 0) {
        printf("读取失败\n");
        return 1;
    }

    printf("c_main 执行完成\n");
    return 0;
}