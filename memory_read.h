#ifndef MEMORY_READ_H
#define MEMORY_READ_H

#include <mach/mach.h>

// 定义一个类似于 HANDLE 的类型
typedef mach_port_t HANDLE;

// 定义 DWORD 类型（在 macOS 中通常是 unsigned int）
typedef unsigned int DWORD;

// 函数声明
DWORD read_int32(HANDLE process_handle, DWORD address);
long long read_int64(HANDLE process_handle, DWORD address);
float read_float(HANDLE process_handle, DWORD address);
double read_double(HANDLE process_handle, DWORD address);
int read_bytes(HANDLE process_handle, DWORD address, size_t size, unsigned char* result);

#endif // MEMORY_READ_H