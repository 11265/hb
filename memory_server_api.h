#ifndef MEMORY_SERVER_API_H
#define MEMORY_SERVER_API_H

#include <mach/mach.h>
#include <sys/types.h>
#include <stddef.h>

/* 结构体定义 */

// 进程信息结构体
typedef struct {
    int pid;                // 进程ID
    const char *processname; // 进程名称
} ProcessInfo;

// 模块信息结构体
typedef struct {
    uintptr_t base;    // 模块基地址
    int size;          // 模块大小
    int is_64bit;      // 是否为64位模块(1表示是,0表示否)
    char *modulename;  // 模块名称
} ModuleInfo;

/* 函数声明 */

// 调试日志函数
int debug_log(const char *format, ...);

// 获取当前进程ID
pid_t get_pid_native();

// 从指定进程读取内存
ssize_t read_memory_native(int pid, mach_vm_address_t address, mach_vm_size_t size,
                           unsigned char *buffer);

// 向指定进程写入内存
ssize_t write_memory_native(int pid, mach_vm_address_t address, mach_vm_size_t size,
                            unsigned char *buffer);

// 枚举内存区域并写入缓冲区
void enumerate_regions_to_buffer(pid_t pid, char *buffer, size_t buffer_size);

// 枚举进程
ProcessInfo *enumprocess_native(size_t *count);

// 枚举模块
ModuleInfo *enummodule_native(pid_t pid, size_t *count);

#endif // MEMORY_SERVER_API_H