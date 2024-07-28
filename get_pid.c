#include <stdio.h>

// 声明一个全局变量
int global_pid = 0;

void get_pvz_pid() {
    int test_pid = 12345;
    printf("C function setting PID: %d\n", test_pid);
    global_pid = test_pid;
}