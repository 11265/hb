#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

// 函数声明
pid_t get_pid_by_name(const char *process_name);

int main() {
    const char *process_name = "SpringBoard"; // 替换为你需要查找的进程名称
    pid_t pid = get_pid_by_name(process_name);

    if (pid != -1) {
        printf("The PID of %s is %d\n", process_name, pid);
    } else {
        printf("Process %s not found\n", process_name);
    }

    return 0;
}

pid_t get_pid_by_name(const char *process_name) {
    DIR *dir;
    struct dirent *entry;
    char path[256];
    char cmdline[256];
    FILE *fp;
    pid_t pid = -1;

    dir = opendir("/proc");
    if (!dir) {
        perror("opendir");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            // Check if the directory name is a number (i.e., a PID)
            if (isdigit(entry->d_name[0])) {
                snprintf(path, sizeof(path), "/proc/%s/comm", entry->d_name);
                fp = fopen(path, "r");
                if (fp) {
                    if (fgets(cmdline, sizeof(cmdline), fp)) {
                        // Remove trailing newline character
                        cmdline[strcspn(cmdline, "\n")] = '\0';
                        if (strcmp(cmdline, process_name) == 0) {
                            pid = atoi(entry->d_name);
                            fclose(fp);
                            break;
                        }
                    }
                    fclose(fp);
                }
            }
        }
    }

    closedir(dir);
    return pid;
}
