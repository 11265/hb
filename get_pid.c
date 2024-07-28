#include "api.h"

int get_pid_by_name(const char* name) {
    pid_t pids[2048];
    int bytes = proc_listpids(PROC_ALL_PIDS, 0, pids, sizeof(pids));
    int n_pids = bytes / sizeof(pid_t);
    
    printf("Debug: Starting get_pid_by_name for process '%s'\n", name);
    printf("Debug: proc_listpids returned %d PIDs\n", n_pids);

    for (int i = 0; i < n_pids; i++) {
        char proc_name[PROC_PIDPATHINFO_MAXSIZE];
        if (proc_name(pids[i], proc_name, sizeof(proc_name)) <= 0) {
            continue;
        }
        printf("Debug: Checking PID %d, name: %s\n", pids[i], proc_name);
        if (strcmp(proc_name, name) == 0) {
            printf("Debug: Found PID %d\n", pids[i]);
            return pids[i];
        }
    }
    
    printf("Debug: Process not found\n");
    return 0; // 未找到
}