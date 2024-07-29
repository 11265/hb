#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mach/mach.h>
#include <libproc.h>

#define MAX_PROCESS_NAME_LENGTH 1024
#define TARGET_PROCESS_NAME "pvz"

// 通过进程名称获取PID
pid_t get_pid_by_name(const char *process_name) {
    pid_t pids[2048];
    int bytes = proc_listpids(PROC_ALL_PIDS, 0, pids, sizeof(pids));
    int n_proc = bytes / sizeof(pids[0]);
    
    for (int i = 0; i < n_proc; i++) {
        char name[MAX_PROCESS_NAME_LENGTH];
        proc_name(pids[i], name, sizeof(name));
        if (strcmp(name, process_name) == 0) {
            return pids[i];
        }
    }
    return -1;
}

int c_main(void) {
    pid_t target_pid = get_pid_by_name(TARGET_PROCESS_NAME);
    if (target_pid == -1) 
    {
        fprintf(stderr, "未找到进程：%s\n", TARGET_PROCESS_NAME);
        return -1;
    }
    
    printf("找到进程 %s，PID: %d\n", TARGET_PROCESS_NAME, target_pid);
    printf("开始初始化内存访问...\n");

    int result = initialize_memory_module(target_pid);
    if (result != 0) {
        fprintf(stderr, "无法初始化内存模块，错误代码：%d\n", result);
        return -1;
    }
    printf("内存模块初始化成功\n");

    // 读取多个地址的示例
    vm_address_t addresses[] = {0x104bb7c78, 0x104cc8000, 0x104dd9000, 0x104bb7c7c, 0x104bb7c80};
    for (int i = 0; i < sizeof(addresses) / sizeof(vm_address_t); i++) {
        printf("尝试异步读取地址 0x%llx\n", (unsigned long long)addresses[i]);
        int32_t value = 异步读内存i32(addresses[i]);
        printf("地址 0x%llx 处异步读取的 int32_t 值: %d (0x%x)\n", 
               (unsigned long long)addresses[i], value, value);
    }

    cleanup_memory_module();
    printf("清理完成\n");
    return 0;
}