#include "memory_server_api.h"
#include <mach/mach.h>
#include <mach/task.h>
#include <mach/vm_map.h>
#include <mach-o/dyld_images.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <sys/sysctl.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>

// 进程区域文件名获取函数类型定义
typedef int (*PROC_REGIONFILENAME)(int pid, uint64_t address, void *buffer, uint32_t buffersize);
PROC_REGIONFILENAME proc_regionfilename = NULL;

// 调试日志函数
int debug_log(const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "[MEMORYSERVER] ");
    vfprintf(stderr, format, args);
    va_end(args);
    return 0;
}

// 获取当前进程ID
pid_t get_pid_native() {
    return getpid();
}

// 从指定进程读取内存
ssize_t read_memory_native(int pid, mach_vm_address_t address, mach_vm_size_t size,
                           unsigned char *buffer) {
    mach_port_t task;
    kern_return_t kr;
    
    if (pid == getpid()) {
        task = mach_task_self();
    } else {
        kr = task_for_pid(mach_task_self(), pid, &task);
        if (kr != KERN_SUCCESS) {
            debug_log("错误:task_for_pid失败,错误码 %d (%s)\n", kr, mach_error_string(kr));
            return -1;
        }
    }

    vm_size_t out_size = 0;
    kr = vm_read_overwrite(task, (vm_address_t)address, (vm_size_t)size, (vm_address_t)buffer, &out_size);
    if (kr != KERN_SUCCESS) {
        debug_log("错误:vm_read_overwrite失败,错误码 %d (%s)\n", kr,
                  mach_error_string(kr));
        return -1;
    }

    return (ssize_t)out_size;
}

// 向指定进程写入内存
ssize_t write_memory_native(int pid, mach_vm_address_t address, mach_vm_size_t size,
                            unsigned char *buffer) {
    task_t task;
    kern_return_t err;
    vm_prot_t original_protection;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object_name;
    int is_embedded_mode = pid == getpid();

    if (is_embedded_mode) {
        task = mach_task_self();
    } else {
        err = task_for_pid(mach_task_self(), pid, &task);
        if (err != KERN_SUCCESS) {
            debug_log("错误:task_for_pid失败,错误码 %d (%s)\n", err, mach_error_string(err));
            return -1;
        }
    }

    if (!is_embedded_mode) {
        task_suspend(task);
    }

    vm_address_t region_address = (vm_address_t)address;
    vm_size_t region_size = (vm_size_t)size;
    // 获取当前内存保护
    err = vm_region_64(task, &region_address, &region_size, VM_REGION_BASIC_INFO_64,
                       (vm_region_info_t)&info, &info_count, &object_name);
    if (err != KERN_SUCCESS) {
        debug_log("错误:vm_region失败,错误码 %d (%s),地址 0x%llx,大小 0x%llx\n",
                  err, mach_error_string(err), (unsigned long long)address, (unsigned long long)size);
        if (!is_embedded_mode) {
            task_resume(task);
        }
        return -1;
    }
    original_protection = info.protection;

    // 更改内存保护以允许写入
    err = vm_protect(task, (vm_address_t)address, (vm_size_t)size, 0, VM_PROT_READ | VM_PROT_WRITE);
    if (err != KERN_SUCCESS) {
        debug_log("错误:vm_protect(写入使能)失败,错误码 %d (%s)\n", err, mach_error_string(err));
        if (!is_embedded_mode) {
            task_resume(task);
        }
        return -1;
    }

    // 写入内存
    err = vm_write(task, (vm_address_t)address, (vm_offset_t)buffer, (mach_msg_type_number_t)size);
    if (err != KERN_SUCCESS) {
        debug_log("错误:vm_write失败,错误码 %d (%s),地址 0x%llx,大小 0x%llx\n",
                  err, mach_error_string(err), (unsigned long long)address, (unsigned long long)size);
        vm_protect(task, (vm_address_t)address, (vm_size_t)size, 0, original_protection);  // 尝试恢复保护
        if (!is_embedded_mode) {
            task_resume(task);
        }
        return -1;
    }

    // 重置内存保护
    err = vm_protect(task, (vm_address_t)address, (vm_size_t)size, 0, original_protection);
    if (err != KERN_SUCCESS) {
        debug_log("警告:vm_protect(恢复保护)失败,错误码 %d (%s)\n", err, mach_error_string(err));
        if (!is_embedded_mode) {
            task_resume(task);
        }
        return -1;
    }

    if (!is_embedded_mode) {
        task_resume(task);
    }
    return (ssize_t)size;
}

// 枚举内存区域并写入缓冲区
void enumerate_regions_to_buffer(pid_t pid, char *buffer, size_t buffer_size) {
    task_t task;
    kern_return_t err;
    mach_vm_address_t address = 0;
    mach_vm_size_t size = 0;
    natural_t depth = 0;

    if (pid == getpid()) {
        task = mach_task_self();
    } else {
        err = task_for_pid(mach_task_self(), pid, &task);
        if (err != KERN_SUCCESS) {
            debug_log("错误:task_for_pid失败,错误码 %d (%s)\n", err, mach_error_string(err));
            snprintf(buffer, buffer_size, "无法获取pid %d的任务\n", pid);
            return;
        }
    }

    size_t pos = 0;
    while (1) {
        vm_region_submap_info_data_64_t info;
        mach_msg_type_number_t info_count = VM_REGION_SUBMAP_INFO_COUNT_64;

        if (vm_region_recurse_64(task, (vm_address_t*)&address, (vm_size_t*)&size, &depth, (vm_region_info_t)&info, &info_count) != KERN_SUCCESS) {
            break;
        }

        if (info.is_submap) {
            depth++;
        } else {
            char protection[4] = "---";
            if (info.protection & VM_PROT_READ) protection[0] = 'r';
            if (info.protection & VM_PROT_WRITE) protection[1] = 'w';
            if (info.protection & VM_PROT_EXECUTE) protection[2] = 'x';

            pos += snprintf(buffer + pos, buffer_size - pos, "%llx-%llx %s\n",
                            (unsigned long long)address,
                            (unsigned long long)(address + size), protection);

            if (pos >= buffer_size - 1) break;

            address += size;
        }
    }
}

// 枚举进程
ProcessInfo* enumprocess_native(size_t* count) {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    size_t size;
    struct kinfo_proc *procs;
    ProcessInfo *result = NULL;
    size_t i, nprocs;

    if (sysctl(mib, 4, NULL, &size, NULL, 0) < 0) {
        debug_log("错误:sysctl获取进程列表大小失败: %s\n", strerror(errno));
        *count = 0;
        return NULL;
    }

    procs = malloc(size);
    if (!procs) {
        debug_log("错误:malloc失败\n");
        *count = 0;
        return NULL;
    }

    if (sysctl(mib, 4, procs, &size, NULL, 0) < 0) {
        debug_log("错误:sysctl获取进程列表失败: %s\n", strerror(errno));
        free(procs);
        *count = 0;
        return NULL;
    }

    nprocs = size / sizeof(struct kinfo_proc);
    result = malloc(nprocs * sizeof(ProcessInfo));
    if (!result) {
        debug_log("错误:malloc失败\n");
        free(procs);
        *count = 0;
        return NULL;
    }

    for (i = 0; i < nprocs; i++) {
        result[i].pid = procs[i].kp_proc.p_pid;
        strncpy(result[i].processname, procs[i].kp_proc.p_comm, sizeof(result[i].processname) - 1);
        result[i].processname[sizeof(result[i].processname) - 1] = '\0';
    }

    free(procs);
    *count = nprocs;
    return result;
}



// 枚举模块
ModuleInfo* enummodule_native(int pid, size_t* count) {
    task_t task;
    kern_return_t kr;
    struct task_dyld_info dyld_info;
    mach_msg_type_number_t count_info = TASK_DYLD_INFO_COUNT;
    ModuleInfo* modules = NULL;
    size_t module_count = 0;

    *count = 0;

    kr = task_for_pid(mach_task_self(), pid, &task);
    if (kr != KERN_SUCCESS) {
        debug_log("错误:task_for_pid失败,错误码 %d (%s)\n", kr, mach_error_string(kr));
        return NULL;
    }

    kr = task_info(task, TASK_DYLD_INFO, (task_info_t)&dyld_info, &count_info);
    if (kr != KERN_SUCCESS) {
        debug_log("错误:task_info失败,错误码 %d (%s)\n", kr, mach_error_string(kr));
        return NULL;
    }

    struct dyld_all_image_infos all_image_infos_data;
    vm_size_t read_size;
    kr = vm_read_overwrite(task, dyld_info.all_image_info_addr,
                           sizeof(struct dyld_all_image_infos),
                           (vm_address_t)&all_image_infos_data, &read_size);
    if (kr != KERN_SUCCESS) {
        debug_log("错误:vm_read_overwrite失败,错误码 %d (%s)\n", kr, mach_error_string(kr));
        return NULL;
    }

    uint32_t image_count = all_image_infos_data.infoArrayCount;
    struct dyld_image_info* image_infos = malloc(sizeof(struct dyld_image_info) * image_count);
    if (!image_infos) {
        debug_log("错误:malloc失败\n");
        return NULL;
    }

    kr = vm_read_overwrite(task, (vm_address_t)all_image_infos_data.infoArray,
                           sizeof(struct dyld_image_info) * image_count,
                           (vm_address_t)image_infos, &read_size);
    if (kr != KERN_SUCCESS) {
        debug_log("错误:vm_read_overwrite失败,错误码 %d (%s)\n", kr, mach_error_string(kr));
        free(image_infos);
        return NULL;
    }

    modules = malloc(sizeof(ModuleInfo) * image_count);
    if (!modules) {
        debug_log("错误:malloc失败\n");
        free(image_infos);
        return NULL;
    }

    for (uint32_t i = 0; i < image_count; i++) {
        char path[PATH_MAX];
        kr = vm_read_overwrite(task, (vm_address_t)image_infos[i].imageFilePath,
                               PATH_MAX, (vm_address_t)path, &read_size);
if (kr != KERN_SUCCESS) {
            debug_log("错误:vm_read_overwrite失败,错误码 %d (%s)\n", kr, mach_error_string(kr));
            continue;
        }

        path[PATH_MAX - 1] = '\0';
        const char* filename = strrchr(path, '/');
        if (filename) {
            filename++;
        } else {
            filename = path;
        }

        strncpy(modules[module_count].modulename, filename, sizeof(modules[module_count].modulename) - 1);
        modules[module_count].modulename[sizeof(modules[module_count].modulename) - 1] = '\0';
        modules[module_count].baseaddress = (uint64_t)image_infos[i].imageLoadAddress;
        module_count++;
    }

    free(image_infos);
    *count = module_count;
    return modules;
}

// 获取模块信息
int get_module_information_native(int pid, const char* modulename, ModuleInfo* info) {
    size_t count;
    ModuleInfo* modules = enummodule_native(pid, &count);
    if (!modules) {
        return -1;
    }

    for (size_t i = 0; i < count; i++) {
        if (strcmp(modules[i].modulename, modulename) == 0) {
            *info = modules[i];
            free(modules);
            return 0;
        }
    }

    free(modules);
    return -1;
}

// 初始化函数
__attribute__((constructor))
static void initialize() {
    void* handle = dlopen("/usr/lib/libproc.dylib", RTLD_LAZY);
    if (handle) {
        proc_regionfilename = (PROC_REGIONFILENAME)dlsym(handle, "proc_regionfilename");
        if (!proc_regionfilename) {
            debug_log("警告:无法加载proc_regionfilename函数\n");
        }
    } else {
        debug_log("警告:无法加载libproc.dylib\n");
    }
}

// 获取内存区域文件名
char* get_region_filename(int pid, uint64_t address) {
    if (!proc_regionfilename) {
        return NULL;
    }

    char* buffer = malloc(PATH_MAX);
    if (!buffer) {
        return NULL;
    }

    if (proc_regionfilename(pid, address, buffer, PATH_MAX) <= 0) {
        free(buffer);
        return NULL;
    }

    return buffer;
}

// 获取基址
uint64_t get_base_address(int pid, const char* module_name) {
    ModuleInfo info;
    if (get_module_information_native(pid, module_name, &info) == 0) {
        return info.baseaddress;
    }
    return 0;
}

// 获取模块大小
uint64_t get_module_size(int pid, const char* module_name) {
    task_t task;
    kern_return_t kr;
    mach_vm_address_t address;
    mach_vm_size_t size;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object_name;

    kr = task_for_pid(mach_task_self(), pid, &task);
    if (kr != KERN_SUCCESS) {
        debug_log("错误:task_for_pid失败,错误码 %d (%s)\n", kr, mach_error_string(kr));
        return 0;
    }

    address = get_base_address(pid, module_name);
    if (address == 0) {
        return 0;
    }

    kr = vm_region_64(task, &address, &size, VM_REGION_BASIC_INFO_64,
                      (vm_region_info_t)&info, &info_count, &object_name);
    if (kr != KERN_SUCCESS) {
        debug_log("错误:vm_region_64失败,错误码 %d (%s)\n", kr, mach_error_string(kr));
        return 0;
    }

    return size;
}