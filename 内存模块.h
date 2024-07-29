#ifndef MEMORY_READ_H                                           // 防止头文件重复包含
#define MEMORY_READ_H

#include <mach/mach.h>                                          // 包含Mach相关的头文件
#include <sys/types.h>                                          // 包含系统类型定义，如pid_t

int initialize_task_port(pid_t pid);                            // 初始化任务端口的函数声明
void cleanup_task_port();                                       // 清理任务端口的函数声明
int read_int32(mach_vm_address_t address, int32_t *value);      // 读取32位整数的函数声明
int 读i64(mach_vm_address_t address, int64_t *value);      // 读取64位整数的函数声明

#endif                                                          // 结束头文件保护