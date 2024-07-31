#ifndef MEMORY_MODULE_H
#define MEMORY_MODULE_H

#include <sys/types.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>

typedef struct {
    void* mapped_address;
    size_t mapped_size;
    uintptr_t target_address;
    struct timespec last_update;
    int update_interval_ms;
} MappedMemory;

int     初始化内存模块(pid_t pid);
void     关闭内存模块();
MappedMemory* 创建内存映射(uintptr_t target_address, size_t size, int update_interval_ms);
void    释放内存映射(MappedMemory* mapped_memory);
int     更新内存映射(MappedMemory* mapped_memory);
int32_t 读内存i32(MappedMemory* mapped_memory, uintptr_t offset);
int64_t 读内存i64(MappedMemory* mapped_memory, uintptr_t offset);
float   读内存f32(MappedMemory* mapped_memory, uintptr_t offset);
double  读内存f64(MappedMemory* mapped_memory, uintptr_t offset);

#endif // MEMORY_MODULE_H