#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <errno.h>

static pid_t target_pid;

int 初始化内存模块(pid_t pid) {
    target_pid = pid;
    return 0;
}

void 关闭内存模块() {
    // 不需要特别的清理操作
}

MappedMemory* 创建内存映射(uintptr_t target_address, size_t size, int update_interval_ms) {
    MappedMemory* result = malloc(sizeof(MappedMemory));
    if (!result) {
        perror("Failed to allocate MappedMemory");
        return NULL;
    }

    void* mapped_address = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped_address == MAP_FAILED) {
        perror("Failed to create anonymous mapping");
        free(result);
        return NULL;
    }

    result->mapped_address = mapped_address;
    result->mapped_size = size;
    result->target_address = target_address;
    clock_gettime(CLOCK_MONOTONIC, &result->last_update);
    result->update_interval_ms = update_interval_ms;

    // 初始更新
    更新内存映射(result);

    return result;
}

void 释放内存映射(MappedMemory* mapped_memory) {
    if (mapped_memory) {
        if (mapped_memory->mapped_address) {
            munmap(mapped_memory->mapped_address, mapped_memory->mapped_size);
        }
        free(mapped_memory);
    }
}

int 更新内存映射(MappedMemory* mapped_memory) {
    struct iovec local[1];
    struct iovec remote[1];

    local[0].iov_base = mapped_memory->mapped_address;
    local[0].iov_len = mapped_memory->mapped_size;
    remote[0].iov_base = (void*)mapped_memory->target_address;
    remote[0].iov_len = mapped_memory->mapped_size;

    ssize_t nread = process_vm_readv(target_pid, local, 1, remote, 1, 0);
    if (nread == -1) {
        perror("Failed to read process memory");
        return -1;
    }
    if ((size_t)nread != mapped_memory->mapped_size) {
        fprintf(stderr, "Partial read: %zd of %zu bytes\n", nread, mapped_memory->mapped_size);
        return -1;
    }

    clock_gettime(CLOCK_MONOTONIC, &mapped_memory->last_update);
    return 0;
}

static inline int 检查并更新映射(MappedMemory* mapped_memory) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long elapsed_ms = (now.tv_sec - mapped_memory->last_update.tv_sec) * 1000 +
                      (now.tv_nsec - mapped_memory->last_update.tv_nsec) / 1000000;

    if (elapsed_ms >= mapped_memory->update_interval_ms) {
        return 更新内存映射(mapped_memory);
    }
    return 0;
}

int32_t 读内存i32(MappedMemory* mapped_memory, uintptr_t offset) {
    if (检查并更新映射(mapped_memory) != 0) {
        return 0;
    }
    if (offset + sizeof(int32_t) > mapped_memory->mapped_size) {
        return 0;
    }
    return *(int32_t*)((char*)mapped_memory->mapped_address + offset);
}

int64_t 读内存i64(MappedMemory* mapped_memory, uintptr_t offset) {
    if (检查并更新映射(mapped_memory) != 0) {
        return 0;
    }
    if (offset + sizeof(int64_t) > mapped_memory->mapped_size) {
        return 0;
    }
    return *(int64_t*)((char*)mapped_memory->mapped_address + offset);
}

float 读内存f32(MappedMemory* mapped_memory, uintptr_t offset) {
    if (检查并更新映射(mapped_memory) != 0) {
        return 0.0f;
    }
    if (offset + sizeof(float) > mapped_memory->mapped_size) {
        return 0.0f;
    }
    return *(float*)((char*)mapped_memory->mapped_address + offset);
}

double 读内存f64(MappedMemory* mapped_memory, uintptr_t offset) {
    if (检查并更新映射(mapped_memory) != 0) {
        return 0.0;
    }
    if (offset + sizeof(double) > mapped_memory->mapped_size) {
        return 0.0;
    }
    return *(double*)((char*)mapped_memory->mapped_address + offset);
}