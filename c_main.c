#include <sys/types.h>  // 包含 pid_t
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
    if (is_task_port_initialized) {
        return KERN_SUCCESS; // 已经初始化过
    }

    kern_return_t kret = task_for_pid(mach_task_self(), pid, &global_task_port);
    if (kret != KERN_SUCCESS) {
        printf("task_for_pid 失败: %s\n", mach_error_string(kret));
        return kret;
    }
    is_task_port_initialized = true;
    return KERN_SUCCESS;
}

// 函数：释放全局任务端口
static void cleanup_task_port() {
    if (is_task_port_initialized) {
        mach_port_deallocate(mach_task_self(), global_task_port);
        is_task_port_initialized = false;
    }
}

// 函数：从任务端口读取内存
static kern_return_t read_memory_from_task(mach_vm_address_t address, size_t size, void **buffer) {
    vm_size_t data_size = size;  // 使用 vm_size_t
    vm_address_t read_buffer;    // 使用 vm_address_t
    kern_return_t kret = vm_read_overwrite(global_task_port, address, size, &read_buffer, &data_size);
    if (kret != KERN_SUCCESS) {
        printf("vm_read_overwrite 失败: %s\n", mach_error_string(kret));
        return kret;
    }

    *buffer = (void *)read_buffer;
    return KERN_SUCCESS;
}

// 函数：释放资源
static void release_resources(void *buffer, vm_size_t size) {  // 使用 vm_size_t
    vm_deallocate(mach_task_self(), (vm_address_t)buffer, size);
}

// 函数：读取 int32_t 数据
int read_int32(mach_vm_address_t address, int32_t *value) {
    if (!is_task_port_initialized) {
        if (initialize_task_port(TARGET_PID) != KERN_SUCCESS) {
            return -1;
        }
    }

    void *read_buffer = NULL;
    kern_return_t kret = read_memory_from_task(address, sizeof(int32_t), &read_buffer);
    if (kret != KERN_SUCCESS) {
        return -1;
    }

    memcpy(value, read_buffer, sizeof(int32_t));
    release_resources(read_buffer, sizeof(int32_t));
    return 0;
}

// 函数：读取 int64_t 数据
int read_int64(mach_vm_address_t address, int64_t *value) {
    if (!is_task_port_initialized) {
        if (initialize_task_port(TARGET_PID) != KERN_SUCCESS) {
            return -1;
        }
    }

    void *read_buffer = NULL;
    kern_return_t kret = read_memory_from_task(address, sizeof(int64_t), &read_buffer);
    if (kret != KERN_SUCCESS) {
        return -1;
    }

    memcpy(value, read_buffer, sizeof(int64_t));
    release_resources(read_buffer, sizeof(int64_t));
    return 0;
}

// 主函数
int c_main() {
    printf("目标进程 PID: %d\n", TARGET_PID);
    printf("目标内存地址: 0x%lx\n", (unsigned long)TARGET_ADDRESS);

    // 初始化任务端口
    if (initialize_task_port(TARGET_PID) != KERN_SUCCESS) {
        printf("初始化任务端口失败\n");
        return -1;
    }

    // 读取 int32_t 数据
    int32_t int32_value;
    if (read_int32(TARGET_ADDRESS, &int32_value) == 0) {
        printf("读取 int32_t 数据成功: %d\n", int32_value);
    } else {
        printf("读取 int32_t 数据失败\n");
    }

    // 读取 int64_t 数据
    int64_t int64_value;
    if (read_int64(TARGET_ADDRESS, &int64_value) == 0) {
        printf("读取 int64_t 数据成功: %lld\n", int64_value);
    } else {
        printf("读取 int64_t 数据失败\n");
    }

    // 释放全局任务端口
    cleanup_task_port();

    printf("c_main 执行完成\n");
    return 0;
}



/*
void 初始化数据()
{
	hWindows = FindWindow(NULL, L"地下城与勇士");
	if (hWindows == NULL) {
		MessageBox(NULL, TEXT("未找到该窗口！"), TEXT("错误"), MB_OK);
		return;
	}
	ThreadID = GetWindowThreadProcessId(hWindows, &ProcessID);
	if (ProcessID == NULL) {
		MessageBox(NULL, TEXT("获取进程id失败！"), TEXT("错误"), MB_OK);
		return;
	}
	进程ID = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ProcessID);
	if (进程ID == NULL) {
		MessageBox(NULL, TEXT("获取进程id失败！"), TEXT("错误"), MB_OK);
		return;
	}
} 
