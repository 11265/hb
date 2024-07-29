#include <mach/mach.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

// 定义目标 PID 和内存地址
#define TARGET_PID 22496
#define TARGET_ADDRESS 0x102DD2404

static mach_port_t global_task_port;
static bool is_task_port_initialized = false;

// 函数：初始化全局任务端口
static kern_return_t initialize_task_port(pid_t pid) {
    kern_return_t kret = task_for_pid(mach_task_self(), pid, &global_task_port);
    if (kret != KERN_SUCCESS) {
        printf("task_for_pid 失败: %s\n", mach_error_string(kret));
        return kret;
    }
    is_task_port_initialized = true;
    return KERN_SUCCESS;
}

// 函数：从任务端口读取内存
static kern_return_t read_memory_from_task(mach_vm_address_t address, size_t size, void **buffer) {
    mach_vm_size_t data_size = size;
    vm_offset_t read_buffer;
    kern_return_t kret = mach_vm_read(global_task_port, address, size, &read_buffer, &data_size);
    if (kret != KERN_SUCCESS) {
        printf("mach_vm_read 失败: %s\n", mach_error_string(kret));
        return kret;
    }

    *buffer = (void *)read_buffer;
    return KERN_SUCCESS;
}

// 函数：复制内存
static void copy_memory(void *source, void *destination, size_t size) {
    memcpy(destination, source, size);
}

// 函数：释放资源
static void release_resources(void *buffer, mach_vm_size_t size) {
    vm_deallocate(mach_task_self(), (vm_address_t)buffer, size);
}

// 函数：读取内存
int read_memory(pid_t pid, mach_vm_address_t address, void *buffer, size_t size) {
    if (!is_task_port_initialized) {
        kern_return_t kret = initialize_task_port(pid);
        if (kret != KERN_SUCCESS) {
            return -1;
        }
    }

    void *read_buffer = NULL;
    kern_return_t kret = read_memory_from_task(address, size, &read_buffer);
    if (kret != KERN_SUCCESS) {
        return -1;
    }

    copy_memory(read_buffer, buffer, size);
    release_resources(read_buffer, size);

    return 0;
}

// 主函数
int c_main() {
    printf("目标进程 PID: %d\n", TARGET_PID);
    printf("目标内存地址: 0x%llx\n", TARGET_ADDRESS);

    // 在这里调用读取内存的函数示例
    char buffer[256];
    if (read_memory(TARGET_PID, TARGET_ADDRESS, buffer, sizeof(buffer)) == 0) {
        printf("内存读取成功\n");
    } else {
        printf("内存读取失败\n");
    }

    printf("c_main 执行完成\n");
    return 0;
}
