#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>
#include <mach/mach_error.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

static void *mapped_memory = NULL;
static size_t mapped_size = 0;
static mach_vm_address_t base_address = 0;

int initialize_memory_access(pid_t pid, mach_vm_address_t address, size_t size) {
    char mem_file[64];
    snprintf(mem_file, sizeof(mem_file), "/proc/%d/mem", pid);
    
    int fd = open(mem_file, O_RDWR);
    if (fd == -1) {
        perror("open failed");
        return -1;
    }

    mapped_memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, address);
    if (mapped_memory == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        return -1;
    }

    close(fd);
    mapped_size = size;
    base_address = address;
    return 0;
}

void cleanup_memory_access() {
    if (mapped_memory != NULL) {
        munmap(mapped_memory, mapped_size);
        mapped_memory = NULL;
        mapped_size = 0;
        base_address = 0;
    }
}

static void* get_mapped_address(mach_vm_address_t address) {
    if (mapped_memory == NULL || address < base_address || address >= base_address + mapped_size) {
        return NULL;
    }
    return (void*)((char*)mapped_memory + (address - base_address));
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

// 写入函数
void 写内存i32(mach_vm_address_t address, int32_t value) {
    void* mapped_addr = get_mapped_address(address);
    if (mapped_addr != NULL) *(int32_t*)mapped_addr = value;
}

void 写内存i64(mach_vm_address_t address, int64_t value) {
    void* mapped_addr = get_mapped_address(address);
    if (mapped_addr != NULL) *(int64_t*)mapped_addr = value;
}

void 写内存f32(mach_vm_address_t address, float value) {
    void* mapped_addr = get_mapped_address(address);
    if (mapped_addr != NULL) *(float*)mapped_addr = value;
}

void 写内存f64(mach_vm_address_t address, double value) {
    void* mapped_addr = get_mapped_address(address);
    if (mapped_addr != NULL) *(double*)mapped_addr = value;
}