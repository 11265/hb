#include <stdio.h>

// 声明一个全局变量
int global_pid = 12345;

void get_pvz_pid() {
    printf("C function: global_pid = %d\n", global_pid);
    printf("C function: &global_pid = %p\n", (void*)&global_pid);
}