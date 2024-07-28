#include <stdio.h>

int get_pvz_pid() {
    int test_pid = 12345;
    printf("C function returning PID: %d\n", test_pid);
    return test_pid;
}