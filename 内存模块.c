#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

static void *mapped_memory = NULL;
static size_t mapped_size = 0;
static off_t base_offset = 0;

int initialize_memory_access(pid_t pid, mach_vm_address_t address, size_t size) {
    char mem_file[64];
    snprintf(mem_file, sizeof(mem_file), "/proc/%d/mem", pid);
    
    int fd = open(mem_file, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "open failed: %s\n", strerror(errno));
        return -1;
    }

    // 计算页面对齐的地址和大小
    long page_size = sysconf(_SC_PAGESIZE);
    off_t page_aligned_offset = (address / page_size) * page_size;
    size_t page_aligned_size = size + (address - page_aligned_offset);

    mapped_memory = mmap(NULL, page_aligned_size, PROT_READ, MAP_PRIVATE, fd, page_aligned_offset);
    if (mapped_memory == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    mapped_size = page_aligned_size;
    base_offset = address - page_aligned_offset;
    return 0;
}

void cleanup_memory_access() {
    if (mapped_memory != NULL) {
        munmap(mapped_memory, mapped_size);
        mapped_memory = NULL;
        mapped_size = 0;
        base_offset = 0;
    }
}

static void* get_mapped_address(mach_vm_address_t address) {
    if (mapped_memory == NULL || address < base_offset || address >= base_offset + mapped_size) {
        return NULL;
    }
    return (void*)((char*)mapped_memory + (address - base_offset));
}

// 读取函数
int32_t 读内存i32(mach_vm_address_t address) {
    void* mapped_addr = get_mapped_address(address);
    if (mapped_addr == NULL) return 0;
    return *(int32_t*)mapped_addr;
}

int64_t 读内存i64(mach_vm_address_t address) {
    void* mapped_addr = get_mapped_address(address);
    if (mapped_addr == NULL) return 0;
    return *(int64_t*)mapped_addr;
}

float 读内存f32(mach_vm_address_t address) {
    void* mapped_addr = get_mapped_address(address);
    if (mapped_addr == NULL) return 0.0f;
    return *(float*)mapped_addr;
}

double 读内存f64(mach_vm_address_t address) {
    void* mapped_addr = get_mapped_address(address);
    if (mapped_addr == NULL) return 0.0;
    return *(double*)mapped_addr;
}

// 注意：移除了写入函数，因为mmap是以只读方式打开的