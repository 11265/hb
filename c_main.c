#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <stdbool.h>

#define TARGET_PROCESS_NAME "pvz"
#define PROC_PIDPATHINFO_MAXSIZE  (4 * MAXPATHLEN)

typedef struct kinfo_proc kinfo_proc;

static int get_proc_list(kinfo_proc **procList, size_t *procCount) {
    int                 err;
    kinfo_proc *        result;
    bool                done;
    static const int    name[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
    size_t              length;

    *procCount = 0;

    result = NULL;
    done = false;
    do {
        length = 0;
        err = sysctl((int *)name, (sizeof(name) / sizeof(*name)) - 1,
                     NULL, &length,
                     NULL, 0);
        if (err == -1) {
            err = errno;
        }

        if (err == 0) {
            result = malloc(length);
            if (result == NULL) {
                err = ENOMEM;
            }
        }

        if (err == 0) {
            err = sysctl((int *)name, (sizeof(name) / sizeof(*name)) - 1,
                         result, &length,
                         NULL, 0);
            if (err == -1) {
                err = errno;
            }
            if (err == 0) {
                done = true;
            } else if (err == ENOMEM) {
                free(result);
                result = NULL;
                err = 0;
            }
        }
    } while (err == 0 && !done);

    if (err != 0 && result != NULL) {
        free(result);
        result = NULL;
    }

    if (err == 0) {
        *procList = result;
        *procCount = length / sizeof(kinfo_proc);
    }

    return err;
}

pid_t get_pid_by_name(const char *process_name) {
    kinfo_proc *procList = NULL;
    size_t procCount = 0;
    int err = get_proc_list(&procList, &procCount);
    
    if (err != 0) {
        fprintf(stderr, "无法获取进程列表: %d\n", err);
        return -1;
    }

    pid_t target_pid = -1;
    for (size_t i = 0; i < procCount; i++) {
        if (strcmp(procList[i].kp_proc.p_comm, process_name) == 0) {
            target_pid = procList[i].kp_proc.p_pid;
            break;
        }
    }

    free(procList);
    return target_pid;
}

int c_main(void) {
    pid_t target_pid = get_pid_by_name(TARGET_PROCESS_NAME);
    if (target_pid == -1) {
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