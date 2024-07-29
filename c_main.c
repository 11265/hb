#include "内存模块.h"
#include <stdio.h>

#define TARGET_PID 22496
#define TARGET_ADDRESS 0x102DD2404

int c_main() 
{
    printf("目标进程 PID: %d\n", TARGET_PID);
    printf("目标内存地址: 0x%x\n", TARGET_ADDRESS);

    printf("c_main 执行完成\n");
    return 0;
}
