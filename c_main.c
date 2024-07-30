// c_main.c

#include "内存模块.h"
#include "查找进程.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TARGET_PROCESS_NAME "pvz"
#define START_ADDRESS 0x00000000000
#define END_ADDRESS   0x20000000000

int 是否有效地址(vm_address_t address) {
    void* data = 读任意地址(address, sizeof(int32_t));
    if (data) {
        内存池释放(data);
        return 1;
    }
    return 0;
}

void 扫描内存范围() {
    printf("开始扫描内存范围 0x%llx 到 0x%llx\n", START_ADDRESS, END_ADDRESS);

    for (vm_address_t address = START_ADDRESS; address < END_ADDRESS; address += sizeof(int32_t)) {
        if (是否有效地址(address)) {
            int32_t value = 读内存i32(address);
            printf("地址: 0x%llx, 值: %d\n", (unsigned long long)address, value);
        }
        
        // 每扫描 100MB 输出一次进度
        if (address % (100 * 1024 * 1024) == 0) {
            printf("当前进度: 0x%llx (%.2f%%)\n", (unsigned long long)address, (float)address / END_ADDRESS * 100);
        }
    }

    printf("内存扫描完成\n");
}

int c_main() {
    pid_t target_pid = get_pid_by_name(TARGET_PROCESS_NAME);
    if (target_pid == -1) {
        printf("无法找到目标进程: %s\n", TARGET_PROCESS_NAME);
        return 1;
    }

    printf("找到目标进程 %s, PID: %d\n", TARGET_PROCESS_NAME, target_pid);

    if (初始化内存模块(target_pid) != 0) {
        printf("初始化内存模块失败\n");
        return 1;
    }

    printf("内存模块初始化成功\n");

    // 测试读写不同类型的内存
    vm_address_t test_address = 0x1060E1388; // 假设这是一个有效的内存地址

    // 测试 int32
    int32_t write_value_i32 = 42;
    if (写内存i32(test_address, write_value_i32) == 0) {
        int32_t read_value_i32 = 读内存i32(test_address);
        printf("int32 测试: 写入 %d, 读取 %d\n", write_value_i32, read_value_i32);
    } else {
        printf("int32 写入失败\n");
    }

    // 测试 float
    float write_value_f32 = 3.14f;
    if (写内存f32(test_address, write_value_f32) == 0) {
        float read_value_f32 = 读内存f32(test_address);
        printf("float 测试: 写入 %f, 读取 %f\n", write_value_f32, read_value_f32);
    } else {
        printf("float 写入失败\n");
    }

    // 测试任意大小的内存读写
    char write_buffer[] = "Hello, World!";
    size_t buffer_size = sizeof(write_buffer);
    if (写任意地址(test_address, write_buffer, buffer_size) == 0) {
        char* read_buffer = (char*)读任意地址(test_address, buffer_size);
        if (read_buffer) {
            printf("任意大小测试: 写入 '%s', 读取 '%s'\n", write_buffer, read_buffer);
            内存池释放(read_buffer);
        } else {
            printf("任意大小读取失败\n");
        }
    } else {
        printf("任意大小写入失败\n");
    }

    // 添加新的内存扫描功能
    printf("是否要执行内存扫描? (y/n): ");
    char response;
    scanf(" %c", &response);
    if (response == 'y' || response == 'Y') {
        扫描内存范围();
    }

    关闭内存模块();
    printf("内存模块已关闭\n");

    return 0;
}