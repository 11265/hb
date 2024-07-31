#include "memory_server_api.h"
#include <stdio.h>
#include <stdlib.h>

void test_read_write_memory() {
    pid_t pid = get_pid_native();
    int test_value = 42;
    mach_vm_address_t address = (mach_vm_address_t)&test_value;

    printf("测试读写内存:\n");
    printf("原始值: %d\n", test_value);

    unsigned char buffer[sizeof(int)];
    ssize_t bytes_read = read_memory_native(pid, address, sizeof(int), buffer);
    if (bytes_read == sizeof(int)) {
        int read_value = *(int*)buffer;
        printf("读取的值: %d\n", read_value);
    } else {
        printf("读取内存失败\n");
    }

    int new_value = 100;
    ssize_t bytes_written = write_memory_native(pid, address, sizeof(int), (unsigned char*)&new_value);
    if (bytes_written == sizeof(int)) {
        printf("写入新值: %d\n", new_value);
        printf("修改后的实际值: %d\n", test_value);
    } else {
        printf("写入内存失败\n");
    }
}

void test_enumerate_regions() {
    pid_t pid = get_pid_native();
    char buffer[4096];  // 假设4KB足够存储区域信息

    printf("\n测试枚举内存区域:\n");
    enumerate_regions_to_buffer(pid, buffer, sizeof(buffer));
    printf("%s", buffer);
}

void test_enumerate_processes() {
    size_t count;
    ProcessInfo *processes = enumprocess_native(&count);

    printf("\n测试枚举进程:\n");
    if (processes) {
        for (size_t i = 0; i < count; i++) {
            printf("PID: %d, 进程名: %s\n", processes[i].pid, processes[i].processname);
        }
        // 释放内存
        for (size_t i = 0; i < count; i++) {
            free((void*)processes[i].processname);
        }
        free(processes);
    } else {
        printf("枚举进程失败\n");
    }
}

void test_enumerate_modules() {
    pid_t pid = get_pid_native();
    size_t count;
    ModuleInfo *modules = enummodule_native(pid, &count);

    printf("\n测试枚举模块:\n");
    if (modules) {
        for (size_t i = 0; i < count; i++) {
            printf("基址: 0x%lx, 大小: %d, %s位, 名称: %s\n", 
                   modules[i].base, modules[i].size, 
                   modules[i].is_64bit ? "64" : "32", modules[i].modulename);
        }
        // 释放内存
        for (size_t i = 0; i < count; i++) {
            free(modules[i].modulename);
        }
        free(modules);
    } else {
        printf("枚举模块失败\n");
    }
}

int main() {
    printf("Memory Server 测试程序开始运行\n");

    test_read_write_memory();
    test_enumerate_regions();
    test_enumerate_processes();
    test_enumerate_modules();

    printf("\nMemory Server 测试程序结束\n");
    return 0;
}