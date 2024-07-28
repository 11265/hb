#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <libproc.h>

#define PROCESS_NAME "pvz"
#define INITIAL_PID_BUFFER_SIZE 1024

// 不区分大小写的字符串比较函数
static int strcasecmp_custom(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int diff = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
        if (diff != 0) {
            return diff;
        }
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

// 获取PVZ进程的PID
int get_pvz_pid() {
    int pid_buffer_size = INITIAL_PID_BUFFER_SIZE;
    pid_t *pids = NULL;
    int num_processes = 0;
    int target_pid = -1;

    // 分配初始PID缓冲区
    pids = (pid_t *)malloc(pid_buffer_size * sizeof(pid_t));
    if (pids == NULL) {
        fprintf(stderr, "错误：内存分配失败\n");
        return -1;
    }

    // 获取进程列表
    while ((num_processes = proc_listpids(PROC_ALL_PIDS, 0, pids, pid_buffer_size * sizeof(pid_t))) == pid_buffer_size * sizeof(pid_t)) {
        pid_buffer_size *= 2;
        pid_t *new_pids = (pid_t *)realloc(pids, pid_buffer_size * sizeof(pid_t));
        if (new_pids == NULL) {
            free(pids);
            fprintf(stderr, "错误：内存重新分配失败\n");
            return -1;
        }
        pids = new_pids;
    }

    if (num_processes <= 0) {
        free(pids);
        fprintf(stderr, "错误：调用proc_listpids失败\n");
        return -1;
    }

    num_processes /= sizeof(pid_t);
    printf("总进程数: %d\n", num_processes);

    // 遍历进程列表
    for (int i = 0; i < num_processes; i++) {
        struct proc_bsdinfo proc_info;
        if (proc_pidinfo(pids[i], PROC_PIDTBSDINFO, 0, &proc_info, sizeof(proc_info)) <= 0) {
            continue;  // 跳过无法获取信息的进程
        }

        if (strcasecmp_custom(proc_info.pbi_name, PROCESS_NAME) == 0) {
            target_pid = pids[i];
            break;  // 找到匹配项就退出
        }
    }

    free(pids);
    return target_pid;
}