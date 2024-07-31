#include <mach/mach.h>
#include <mach/vm_map.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// 定义调试日志宏，用于输出带有 [MEMORYSERVER] 前缀的日志信息
#define DEBUG_LOG(format, ...) printf("[MEMORYSERVER] " format, ##__VA_ARGS__)

// 定义内存区域结构体，用于存储内存区域的地址、大小和保护属性
typedef struct {
    vm_address_t address;  // 内存区域的起始地址
    vm_size_t size;        // 内存区域的大小
    vm_prot_t protection;  // 内存区域的保护属性
} MemoryRegion;

// 从指定进程的内存中读取数据
kern_return_t read_process_memory(pid_t pid, vm_address_t address, void *buffer, vm_size_t size) {
    task_t task;
    kern_return_t kr;

    // 如果目标进程是当前进程，直接使用 mach_task_self()
    if (pid == getpid()) {
        task = mach_task_self();
    } else {
        // 否则，通过 pid 获取目标进程的 task
        kr = task_for_pid(mach_task_self(), pid, &task);
        if (kr != KERN_SUCCESS) {
            DEBUG_LOG("错误：task_for_pid 失败，错误码 %d (%s)\n", kr, mach_error_string(kr));
            return kr;
        }
    }

    // 使用 vm_read_overwrite 读取内存
    vm_size_t out_size;
    kr = vm_read_overwrite(task, address, size, (vm_address_t)buffer, &out_size);
    if (kr != KERN_SUCCESS) {
        DEBUG_LOG("错误：vm_read_overwrite 失败，错误码 %d (%s)\n", kr, mach_error_string(kr));
        return kr;
    }

    return KERN_SUCCESS;
}

// 枚举指定进程的内存区域
void enumerate_memory_regions(pid_t pid) {
    task_t task;
    kern_return_t kr;

    // 获取目标进程的 task
    if (task_for_pid(mach_task_self(), pid, &task) != KERN_SUCCESS) {
        DEBUG_LOG("错误：无法获取进程 %d 的 task\n", pid);
        return;
    }

    vm_address_t address = 0;
    vm_size_t size;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t info_count;
    mach_port_t object_name;

    // 循环遍历所有内存区域
    while (1) {
        info_count = VM_REGION_BASIC_INFO_COUNT_64;
        kr = vm_region_64(task, &address, &size, VM_REGION_BASIC_INFO_64,
                          (vm_region_info_t)&info, &info_count, &object_name);

        if (kr != KERN_SUCCESS) {
            break;  // 遍历结束或发生错误
        }

        // 解析内存保护属性
        char protection[4] = "---";
        if (info.protection & VM_PROT_READ) protection[0] = 'r';
        if (info.protection & VM_PROT_WRITE) protection[1] = 'w';
        if (info.protection & VM_PROT_EXECUTE) protection[2] = 'x';

        // 打印内存区域信息
        printf("区域: 0x%llx - 0x%llx (%s)\n", 
               (unsigned long long)address, 
               (unsigned long long)(address + size), 
               protection);

        // 移动到下一个区域
        address += size;
    }
}

int main(int argc, char *argv[]) {
    // 检查命令行参数
    if (argc != 2) {
        printf("用法: %s <pid>\n", argv[0]);
        return 1;
    }

    // 从命令行参数获取目标进程 ID
    pid_t pid = atoi(argv[1]);
    
    printf("枚举进程 %d 的内存区域:\n", pid);
    enumerate_memory_regions(pid);

    // 从特定地址读取内存的示例
    vm_address_t test_address = 0x1000; // 替换为您想要读取的实际地址
    char buffer[100];
    
    kern_return_t kr = read_process_memory(pid, test_address, buffer, sizeof(buffer));
    if (kr == KERN_SUCCESS) {
        printf("成功从地址 0x%llx 读取 %zu 字节\n", (unsigned long long)test_address, sizeof(buffer));
        // 根据需要处理读取的数据
    } else {
        printf("无法从地址 0x%llx 读取内存\n", (unsigned long long)test_address);
    }

    return 0;
}