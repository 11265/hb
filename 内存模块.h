#ifndef MEMORY_MODULE_H
#define MEMORY_MODULE_H

#include <sys/types.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>

typedef struct {
    void* mapped_address;
    size_t mapped_size;
    int mem_fd;
} MappedMemory;

MappedMemory* 创建内存映射(uintptr_t target_address, size_t size);

int     初始化内存模块(pid_t pid);
void    关闭内存模块();

void    释放内存映射(MappedMemory* mapped_memory);
int32_t 读内存i32(MappedMemory* mapped_memory, uintptr_t offset);
int64_t 读内存i64(MappedMemory* mapped_memory, uintptr_t offset);
float   读内存f32(MappedMemory* mapped_memory, uintptr_t offset);
double  读内存f64(MappedMemory* mapped_memory, uintptr_t offset);

#endif // MEMORY_MODULE_H