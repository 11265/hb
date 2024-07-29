#include <sys/types.h>
#include <mach/mach.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define TARGET_PID 22496
#define TARGET_ADDRESS 0x102DD2404

static mach_port_t global_task_port;
static bool is_task_port_initialized = false;

static kern_return_t initialize_task_port(pid_t pid) {
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

static void cleanup_task_port() {
    if (is_task_port_initialized) {
        mach_port_deallocate(mach_task_self(), global_task_port);
        is_task_port_initialized = false;
    }
}

static kern_return_t read_memory_from_task(mach_vm_address_t address, size_t size, void *buffer) {
    vm_size_t data_size = size;
    kern_return_t kret = vm_read_overwrite(global_task_port, address, size, (vm_address_t)buffer, &data_size);
    if (kret != KERN_SUCCESS) {
        printf("vm_read_overwrite 失败: %s\n", mach_error_string(kret));
        return kret;
    }
    return KERN_SUCCESS;
}

int read_int32(mach_vm_address_t address, int32_t *value) {
    if (!is_task_port_initialized) {
        if (initialize_task_port(TARGET_PID) != KERN_SUCCESS) {
            return -1;
        }
    }

    kern_return_t kret = read_memory_from_task(address, sizeof(int32_t), value);
    if (kret != KERN_SUCCESS) {
        return -1;
    }
    return 0;
}

int read_int64(mach_vm_address_t address, int64_t *value) {
    if (!is_task_port_initialized) {
        if (initialize_task_port(TARGET_PID) != KERN_SUCCESS) {
            return -1;
        }
    }

    kern_return_t kret = read_memory_from_task(address, sizeof(int64_t), value);
    if (kret != KERN_SUCCESS) {
        return -1;
    }
    return 0;
}

int c_main() {
    printf("目标进程 PID: %d\n", TARGET_PID);
    printf("目标内存地址: 0x%llx\n", (unsigned long long)TARGET_ADDRESS);

    if (initialize_task_port(TARGET_PID) != KERN_SUCCESS) {
        printf("初始化任务端口失败\n");
        return -1;
    }

    int32_t int32_value;
    if (read_int32(TARGET_ADDRESS, &int32_value) == 0) {
        printf("读取 int32_t 数据成功: %d\n", int32_value);
    } else {
        printf("读取 int32_t 数据失败\n");
    }

    int64_t int64_value;
    if (read_int64(TARGET_ADDRESS, &int64_value) == 0) {
        printf("读取 int64_t 数据成功: %lld\n", int64_value);
    } else {
        printf("读取 int64_t 数据失败\n");
    }

    cleanup_task_port();

    printf("c_main 执行完成\n");
    return 0;
}
