#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>
#include <mach/mach_error.h>
#include <stdbool.h>

static mach_port_t global_task_port;
static bool is_task_port_initialized = false;

int initialize_task_port(pid_t pid) {
    if (is_task_port_initialized) {
        return KERN_SUCCESS;
    }

    kern_return_t kret = task_for_pid(mach_task_self(), pid, &global_task_port);
    if (kret != KERN_SUCCESS) {
        printf("task_for_pid 失败: %s\n", mach_error_string(kret));
        return kret;
    }
    is_task_port_initialized = true;
    return KERN_SUCCESS;
}

void cleanup_task_port() {
    if (is_task_port_initialized) {
        mach_port_deallocate(mach_task_self(), global_task_port);
        is_task_port_initialized = false;
    }
}

static void read_memory_from_task(mach_vm_address_t address, size_t size, void *buffer) {
    vm_size_t data_size = size;
    vm_read_overwrite(global_task_port, address, size, (vm_address_t)buffer, &data_size);
}

static void write_memory_to_task(mach_vm_address_t address, size_t size, void *data) {
    vm_write(global_task_port, address, (vm_offset_t)data, size);
}

// 读取函数
int32_t 读内存i32(mach_vm_address_t address) {
    int32_t value = 0;
    read_memory_from_task(address, sizeof(int32_t), &value);
    return value;
}

int64_t 读内存i64(mach_vm_address_t address) {
    int64_t value = 0;
    read_memory_from_task(address, sizeof(int64_t), &value);
    return value;
}

float 读内存f32(mach_vm_address_t address) {
    float value = 0.0f;
    read_memory_from_task(address, sizeof(float), &value);
    return value;
}

double 读内存f64(mach_vm_address_t address) {
    double value = 0.0;
    read_memory_from_task(address, sizeof(double), &value);
    return value;
}

// 写入函数
void 写内存i32(mach_vm_address_t address, int32_t value) {
    write_memory_to_task(address, sizeof(int32_t), &value);
}

void 写内存i64(mach_vm_address_t address, int64_t value) {
    write_memory_to_task(address, sizeof(int64_t), &value);
}

void 写内存f32(mach_vm_address_t address, float value) {
    write_memory_to_task(address, sizeof(float), &value);
}

void 写内存f64(mach_vm_address_t address, double value) {
    write_memory_to_task(address, sizeof(double), &value);
}