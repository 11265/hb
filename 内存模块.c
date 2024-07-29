#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>
#include <mach/mach_error.h>

// 定义全局变量 target_pid
pid_t target_pid = 22496;  // 替换为实际的 PID

static mach_port_t global_task_port;//任务端口