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

    mach_vm_size_t out_size;
    kr = mach_vm_read_overwrite(task, address, size, (mach_vm_address_t)buffer, &out_size);
    if (kr != KERN_SUCCESS) {
        debug_log("错误:mach_vm_read_overwrite失败,错误码 %d (%s)\n", kr,
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

    mach_vm_address_t region_address = address;
    mach_vm_size_t region_size = size;
    // 获取当前内存保护
    err = mach_vm_region(task, &region_address, &region_size, VM_REGION_BASIC_INFO_64,
                         (vm_region_info_t)&info, &info_count, &object_name);
    if (err != KERN_SUCCESS) {
        debug_log("错误:mach_vm_region失败,错误码 %d (%s),地址 0x%llx,大小 0x%llx\n",
                  err, mach_error_string(err), address, size);
        if (!is_embedded_mode) {
            task_resume(task);
        }
        return -1;
    }
    original_protection = info.protection;

    // 更改内存保护以允许写入
    err = mach_vm_protect(task, address, size, 0, VM_PROT_READ | VM_PROT_WRITE);
    if (err != KERN_SUCCESS) {
        debug_log("错误:mach_vm_protect(写入使能)失败,错误码 %d (%s)\n", err, mach_error_string(err));
        if (!is_embedded_mode) {
            task_resume(task);
        }
        return -1;
    }

    // 写入内存
    err = mach_vm_write(task, address, (vm_offset_t)buffer, size);
    if (err != KERN_SUCCESS) {
        debug_log("错误:mach_vm_write失败,错误码 %d (%s),地址 0x%llx,大小 0x%llx\n",
                  err, mach_error_string(err), address, size);
        mach_vm_protect(task, address, size, 0, original_protection);  // 尝试恢复保护
        if (!is_embedded_mode) {
            task_resume(task);
        }
        return -1;
    }

    // 重置内存保护
    err = mach_vm_protect(task, address, size, 0, original_protection);
    if (err != KERN_SUCCESS) {
        debug_log("警告:mach_vm_protect(恢复保护)失败,错误码 %d (%s)\n", err, mach_error_string(err));
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
    natural_t depth = 1;

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

        if (vm_region_recurse_64(task, &address, &size, &depth, (vm_region_info_t)&info,
                                 &info_count) != KERN_SUCCESS) {
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
ProcessInfo *enumprocess_native(size_t *count) {
    int err;
    struct kinfo_proc *result;
    int done;
    static const int name[] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    size_t length;

    result = NULL;
    done = 0;

    do {
        length = 0;
        err = sysctl((int *)name, (sizeof(name) / sizeof(*name)) - 1, NULL, &length, NULL, 0);
        if (err == -1) {
            err = errno;
        }

        if (err == 0) {
            result = (struct kinfo_proc *)malloc(length);
            if (result == NULL) {
                err = ENOMEM;
            }
        }

        if (err == 0) {
            err = sysctl((int *)name, (sizeof(name) / sizeof(*name)) - 1, result, &length, NULL, 0);
            if (err == -1) {
                err = errno;
            }
            if (err == 0) {
                done = 1;
            } else if (err == ENOMEM) {
                free(result);
                result = NULL;
                err = 0;
            }
        }
    } while (err == 0 && !done);

    if (err == 0 && result != NULL) {
        *count = length / sizeof(struct kinfo_proc);
        ProcessInfo *processes = (ProcessInfo *)malloc(*count * sizeof(ProcessInfo));

        for (size_t i = 0; i < *count; i++) {
            processes[i].pid = result[i].kp_proc.p_pid;
            processes[i].processname = strdup(result[i].kp_proc.p_comm);
        }

        free(result);
        return processes;
    } else {
        if (result != NULL) {
            free(result);
        }
        debug_log("错误:枚举进程失败,错误码 %d\n", err);
        return NULL;
    }
}

// 暂停进程
int suspend_process(pid_t pid) {
    task_t task;
    kern_return_t err;
    int is_embedded_mode = pid == getpid();
    if (is_embedded_mode) {
        debug_log("错误:无法暂停自身进程\n");
        return 0;
    }
    err = task_for_pid(mach_task_self(), pid, &task);
    if (err != KERN_SUCCESS) {
        debug_log("错误:task_for_pid失败,错误码 %d (%s)\n", err, mach_error_string(err));
        return 0;
    }
    err = task_suspend(task);
    if (err != KERN_SUCCESS) {
        debug_log("错误:task_suspend失败,错误码 %d (%s)\n", err, mach_error_string(err));
        return 0;
    }

    return 1;
}

// 恢复进程
int resume_process(pid_t pid) {
    task_t task;
    kern_return_t err;
    int is_embedded_mode = pid == getpid();
    if (is_embedded_mode) {
        debug_log("错误:无法恢复自身进程\n");
        return 0;
    }
    err = task_for_pid(mach_task_self(), pid, &task);
    if (err != KERN_SUCCESS) {
        debug_log("错误:task_for_pid失败,错误码 %d (%s)\n", err, mach_error_string(err));
        return 0;
    }
    err = task_resume(task);
    if (err != KERN_SUCCESS) {
        debug_log("错误:task_resume失败,错误码 %d (%s)\n", err, mach_error_string(err));
        return 0;
    }

    return 1;
}

// 获取64位镜像大小
static int get_image_size_64(void *header) {
    struct mach_header_64 *mh = (struct mach_header_64 *)header;
    struct load_command *lc = (struct load_command *)((char *)header + sizeof(struct mach_header_64));
    for (uint32_t i = 0; i < mh->ncmds; i++) {
        if (lc->cmd == LC_SEGMENT_64) {
            struct segment_command_64 *seg = (struct segment_command_64 *)lc;
            if (strcmp(seg->segname, "__PAGEZERO") != 0) {
                return (int)(seg->vmaddr + seg->vmsize);
            }
        }
        lc = (struct load_command *)((char *)lc + lc->cmdsize);
    }
    return 0;
}

// 获取32位镜像大小
static int get_image_size_32(void *header) {
    struct mach_header *mh = (struct mach_header *)header;
    struct load_command *lc = (struct load_command *)((char *)header + sizeof(struct mach_header));
    for (uint32_t i = 0; i < mh->ncmds; i++) {
        if (lc->cmd == LC_SEGMENT) {
            struct segment_command *seg = (struct segment_command *)lc;
            if (strcmp(seg->segname, "__PAGEZERO") != 0) {
                return (int)(seg->vmaddr + seg->vmsize);
            }
        }
        lc = (struct load_command *)((char *)lc + lc->cmdsize);
    }
    return 0;
}

// 获取模块大小
static int get_module_size(void *header, int is_64bit) {
    return is_64bit ? get_image_size_64(header) : get_image_size_32(header);
}

// 枚举模块
ModuleInfo *enummodule_native(pid_t pid, size_t *count) {
    task_t task;
    kern_return_t kr;
    struct task_dyld_info dyld_info;
    mach_msg_type_number_t count_out = TASK_DYLD_INFO_COUNT;
    
    if (pid == getpid()) {
        task = mach_task_self();
    } else {
        kr = task_for_pid(mach_task_self(), pid, &task);
        if (kr != KERN_SUCCESS) {
            debug_log("错误:task_for_pid失败,错误码 %d (%s)\n", kr, mach_error_string(kr));
            return NULL;
        }
    }

    kr = task_info(task, TASK_DYLD_INFO, (task_info_t)&dyld_info, &count_out);
    if (kr != KERN_SUCCESS) {
        debug_log("错误:task_info失败,错误码 %d (%s)\n", kr, mach_error_string(kr));
        return NULL;
    }

    struct dyld_all_image_infos *all_image_infos = (struct dyld_all_image_infos *)dyld_info.all_image_info_addr;
    struct dyld_all_image_infos all_image_infos_data;
    
    mach_vm_size_t read_size;
    kr = mach_vm_read_overwrite(task, (mach_vm_address_t)all_image_infos, sizeof(struct dyld_all_image_infos), 
                                (mach_vm_address_t)&all_image_infos_data, &read_size);
    if (kr != KERN_SUCCESS) {
        debug_log("错误:mach_vm_read_overwrite失败,错误码 %d (%s)\n", kr, mach_error_string(kr));
        return NULL;
    }

    uint32_t image_count = all_image_infos_data.infoArrayCount;
    struct dyld_image_info *image_infos = malloc(sizeof(struct dyld_image_info) * image_count);
    
    kr = mach_vm_read_overwrite(task, (mach_vm_address_t)all_image_infos_data.infoArray, 
                                sizeof(struct dyld_image_info) * image_count, 
                                (mach_vm_address_t)image_infos, &read_size);
    if (kr != KERN_SUCCESS) {
        debug_log("错误:读取镜像信息失败,错误码 %d (%s)\n", kr, mach_error_string(kr));
        free(image_infos);
        return NULL;
    }

    ModuleInfo *modules = malloc(sizeof(ModuleInfo) * image_count);
    *count = image_count;

    for (uint32_t i = 0; i < image_count; i++) {
        char path[PATH_MAX];
        kr = mach_vm_read_overwrite(task, (mach_vm_address_t)image_infos[i].imageFilePath, 
                                    PATH_MAX, (mach_vm_address_t)path, &read_size);
        if (kr != KERN_SUCCESS) {
            debug_log("错误:读取模块路径失败,错误码 %d (%s)\n", kr, mach_error_string(kr));
            continue;
        }

        modules[i].base = (uintptr_t)image_infos[i].imageLoadAddress;
        modules[i].modulename = strdup(path);

        // 读取Mach-O头以确定是否为64位
        struct mach_header header;
        kr = mach_vm_read_overwrite(task, (mach_vm_address_t)image_infos[i].imageLoadAddress, 
                                    sizeof(struct mach_header), (mach_vm_address_t)&header, &read_size);
        if (kr != KERN_SUCCESS) {
            debug_log("错误:读取Mach-O头失败,错误码 %d (%s)\n", kr, mach_error_string(kr));
            modules[i].is_64bit = 0;
            modules[i].size = 0;
            continue;
        }

        modules[i].is_64bit = (header.magic == MH_MAGIC_64 || header.magic == MH_CIGAM_64);
        
        // 读取完整的Mach-O头以获取模块大小
        size_t header_size = modules[i].is_64bit ? sizeof(struct mach_header_64) : sizeof(struct mach_header);
        void *full_header = malloc(header_size + header.sizeofcmds);
        
        kr = mach_vm_read_overwrite(task, (mach_vm_address_t)image_infos[i].imageLoadAddress, 
                                    header_size + header.sizeofcmds, (mach_vm_address_t)full_header, &read_size);
        if (kr != KERN_SUCCESS) {
            debug_log("错误:读取完整Mach-O头失败,错误码 %d (%s)\n", kr, mach_error_string(kr));
            modules[i].size = 0;
        } else {
            modules[i].size = get_module_size(full_header, modules[i].is_64bit);
        }
        
        free(full_header);
    }

    free(image_infos);
    return modules;
}