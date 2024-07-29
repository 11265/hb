#include "查找进程.h"

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