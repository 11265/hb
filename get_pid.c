#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>

#define PROCESS_NAME "pvz"

struct kinfo_proc {
    struct extern_proc kp_proc;
    struct eproc {
        char e_paddr[8];
        char e_spare[4];
    } kp_eproc;
};

int get_pvz_pid() {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    size_t size;
    int err;

    err = sysctl(mib, 4, NULL, &size, NULL, 0);
    if (err == -1) {
        perror("sysctl");
        return -1;
    }

    struct kinfo_proc *proc_list = malloc(size);
    if (proc_list == NULL) {
        perror("malloc");
        return -1;
    }

    err = sysctl(mib, 4, proc_list, &size, NULL, 0);
    if (err == -1) {
        perror("sysctl");
        free(proc_list);
        return -1;
    }

    int num_proc = size / sizeof(struct kinfo_proc);
    for (int i = 0; i < num_proc; i++) {
        if (strcmp(proc_list[i].kp_proc.p_comm, PROCESS_NAME) == 0) {
            int pid = proc_list[i].kp_proc.p_pid;
            free(proc_list);
            return pid;
        }
    }

    free(proc_list);
    return -1;
}