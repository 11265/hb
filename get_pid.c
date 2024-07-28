#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <errno.h>

#define PROCESS_NAME "pvz"

int get_pvz_pid() {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    size_t size;
    int err;

    // First call to get the size
    if (sysctl(mib, 4, NULL, &size, NULL, 0) == -1) {
        printf("Error in first sysctl call: %s\n", strerror(errno));
        return -1;
    }

    printf("Size of process list: %zu\n", size);

    struct kinfo_proc *proc_list = malloc(size);
    if (proc_list == NULL) {
        printf("Error in malloc: %s\n", strerror(errno));
        return -1;
    }

    // Second call to get the actual process list
    if (sysctl(mib, 4, proc_list, &size, NULL, 0) == -1) {
        printf("Error in second sysctl call: %s\n", strerror(errno));
        free(proc_list);
        return -1;
    }

    int num_proc = size / sizeof(struct kinfo_proc);
    printf("Number of processes: %d\n", num_proc);

    for (int i = 0; i < num_proc; i++) {
        if (strcmp(proc_list[i].kp_proc.p_comm, PROCESS_NAME) == 0) {
            int pid = proc_list[i].kp_proc.p_pid;
            printf("Found process '%s' with PID: %d\n", PROCESS_NAME, pid);
            free(proc_list);
            return pid;
        }
    }

    printf("Process '%s' not found\n", PROCESS_NAME);
    free(proc_list);
    return -1;
}