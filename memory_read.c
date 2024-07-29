#include <stdio.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>

int read_process_memory() {
    pid_t target_pid;
    mach_vm_address_t target_address;
    mach_vm_size_t size_to_read = 16;  // 读取16字节

    printf("输入目标进程的 PID: ");
    scanf("%d", &target_pid);

    printf("输入要读取的内存地址（十六进制）: ");
    scanf("%llx", &target_address);

    // 获取目标进程的任务端口
    mach_port_t task;
    kern_return_t kr = task_for_pid(mach_task_self(), target_pid, &task);
    if (kr != KERN_SUCCESS) {
        printf("无法获取目标进程的任务端口。错误码: %d\n", kr);
        return 1;
    }

    // 读取内存
    vm_offset_t data;
    mach_msg_type_number_t data_size;
    kr = mach_vm_read(task, target_address, size_to_read, &data, &data_size);
    if (kr != KERN_SUCCESS) {
        printf("无法读取内存。错误码: %d\n", kr);
        return 1;
    }

    // 打印读取的数据
    printf("读取的数据:\n");
    unsigned char *buffer = (unsigned char *)data;
    for (int i = 0; i < data_size; i++) {
        printf("%02X ", buffer[i]);
        if ((i + 1) % 8 == 0) printf("\n");
    }
    printf("\n");

    // 释放资源
    vm_deallocate(mach_task_self(), data, data_size);
    mach_port_deallocate(mach_task_self(), task);

    return 0;
}