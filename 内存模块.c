#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>

static pid_t target_pid;

int 初始化内存模块(pid_t pid) {
    target_pid = pid;
    return 0;
}

void 关闭内存模块() {
    // 不需要特别的清理操作
}

MappedMemory* 创建内存映射(uintptr_t target_address, size_t size) {
    MappedMemory* result = malloc(sizeof(MappedMemory));
    if (!result) {
        perror("Failed to allocate MappedMemory");
        return NULL;
    }

    char mem_file[64];
    snprintf(mem_file, sizeof(mem_file), "/proc/%d/mem", target_pid);
    
    int mem_fd = open(mem_file, O_RDONLY);
    if (mem_fd == -1) {
        perror("Failed to open process memory");
        free(result);
        return NULL;
    }

    void* mapped_address = mmap(NULL, size, PROT_READ, MAP_SHARED, mem_fd, target_address);
    if (mapped_address == MAP_FAILED) {
        perror("Failed to create memory mapping");
        close(mem_fd);
        free(result);
        return NULL;
    }

    result->mapped_address = mapped_address;
    result->mapped_size = size;
    result->mem_fd = mem_fd;

    return result;
}

void 释放内存映射(MappedMemory* mapped_memory) {
    if (mapped_memory) {
        if (mapped_memory->mapped_address) {
            munmap(mapped_memory->mapped_address, mapped_memory->mapped_size);
        }
        if (mapped_memory->mem_fd != -1) {
            close(mapped_memory->mem_fd);
        }
        free(mapped_memory);
    }
}

int32_t 读内存i32(MappedMemory* mapped_memory, uintptr_t offset) {
    if (offset + sizeof(int32_t) > mapped_memory->mapped_size) {
        return 0;
    }
    return *(int32_t*)((char*)mapped_memory->mapped_address + offset);
}

int64_t 读内存i64(MappedMemory* mapped_memory, uintptr_t offset) {
    if (offset + sizeof(int64_t) > mapped_memory->mapped_size) {
        return 0;
    }
    return *(int64_t*)((char*)mapped_memory->mapped_address + offset);
}

float 读内存f32(MappedMemory* mapped_memory, uintptr_t offset) {
    if (offset + sizeof(float) > mapped_memory->mapped_size) {
        return 0.0f;
    }
    return *(float*)((char*)mapped_memory->mapped_address + offset);
}

double 读内存f64(MappedMemory* mapped_memory, uintptr_t offset) {
    if (offset + sizeof(double) > mapped_memory->mapped_size) {
        return 0.0;
    }
    return *(double*)((char*)mapped_memory->mapped_address + offset);
}