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

/* 结构体定义 */

// 进程信息结构体
typedef struct {
    int pid;                // 进程ID
    const char *processname; // 进程名称
} ProcessInfo;

// 模块信息结构体
typedef struct {
    uintptr_t base;    // 模块基地址
    int size;          // 模块大小
    int is_64bit;      // 是否为64位模块（1表示是，0表示否）
    char *modulename;  // 模块名称
} ModuleInfo;

/* 函数原型声明 */

// Mach虚拟内存读取函数
extern kern_return_t mach_vm_read_overwrite(vm_map_t, mach_vm_address_t, mach_vm_size_t,
                                            mach_vm_address_t, mach_vm_size_t *);

// Mach虚拟内存写入函数
extern kern_return_t mach_vm_write(vm_map_t, mach_vm_address_t, vm_offset_t,
                                   mach_msg_type_number_t);

// Mach虚拟内存保护设置函数
extern kern_return_t mach_vm_protect(vm_map_t, mach_vm_address_t, mach_vm_size_t, boolean_t,
                                     vm_prot_t);

// Mach虚拟内存区域信息获取函数
extern kern_return_t mach_vm_region(vm_map_t, mach_vm_address_t *, mach_vm_size_t *,
                                    vm_region_flavor_t, vm_region_info_t,
                                    mach_msg_type_number_t *, mach_port_t *);

// 进程区域文件名获取函数类型定义
typedef int (*PROC_REGIONFILENAME)(int pid, uint64_t address, void *buffer, uint32_t buffersize);
PROC_REGIONFILENAME proc_regionfilename = NULL;

/* 函数实现 */

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
            debug_log("错误：task_for_pid失败，错误码 %d (%s)\n", kr, mach_error_string(kr));
            return -1;
        }
    }

    mach_vm_size_t out_size;
    kr = mach_vm_read_overwrite(task, address, size, (mach_vm_address_t)buffer, &out_size);
    if (kr != KERN_SUCCESS) {
        debug_log("错误：mach_vm_read_overwrite失败，错误码 %d (%s)\n", kr,
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
            debug_log("错误：task_for_pid失败，错误码 %d (%s)\n", err, mach_error_string(err));
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
        debug_log("错误：mach_vm_region失败，错误码 %d (%s)，地址 0x%llx，大小 0x%llx\n",
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
        debug_log("错误：mach_vm_protect（写入使能）失败，错误码 %d (%s)\n", err, mach_error_string(err));
        if (!is_embedded_mode) {
            task_resume(task);
        }
        return -1;
    }

    // 写入内存
    err = mach_vm_write(task, address, (vm_offset_t)buffer, size);
    if (err != KERN_SUCCESS) {
        debug_log("错误：mach_vm_write失败，错误码 %d (%s)，地址 0x%llx，大小 0x%llx\n",
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
        debug_log("警告：mach_vm_protect（恢复保护）失败，错误码 %d (%s)\n", err, mach_error_string(err));
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
            debug_log("错误：task_for_pid失败，错误码 %d (%s)\n", err, mach_error_string(err));
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
        debug_log("错误：枚举进程失败，错误码 %d\n", err);
        return NULL;
    }
}

// 暂停进程
int suspend_process(pid_t pid) {
    task_t task;
    kern_return_t err;
    int is_embedded_mode = pid == getpid();
    if (is_embedded_mode) {
        debug_log("错误：无法暂停自身进程\n");
        return 0;
    }
    err = task_for_pid(mach_task_self(), pid, &task);
    if (err != KERN_SUCCESS) {
        debug_log("错误：task_for_pid失败，错误码 %d (%s)\n", err, mach_error_string(err));
        return 0;
    }
    err = task_suspend(task);
    if (err != KERN_SUCCESS) {
        debug_log("错误：task_suspend失败，错误码 %d (%s)\n", err, mach_error_string(err));
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
        debug_log("错误：无法恢复自身进程\n");
        return 0;
    }
    err = task_for_pid(mach_task_self(), pid, &task);
    if (err != KERN_SUCCESS) {
        debug_log("错误：task_for_pid失败，错误码 %d (%s)\n", err, mach_error_string(err));
        return 0;
    }
    err = task_resume(task);
    if (err != KERN_SUCCESS) {
        debug_log("错误：task_resume失败，错误码 %d (%s)\n", err, mach_error_string(err));
        return 0;
    }

    return 1;
}

// 获取64位镜像大小
static uint64_t get_image_size_64(int pid, mach_vm_address_t base_address) {
    struct mach_header_64 header;
    if (read_memory_native(pid, base_address, sizeof(struct mach_header_64),
                           (unsigned char *)&header) <= 0) {
        debug_log("错误：无法读取64位Mach-O头\n");
        return 0;
    }

uint64_t image_size = 0;
    mach_vm_address_t current_address = base_address + sizeof(struct mach_header_64);

    for (int i = 0; i < header.ncmds; i++) {
        struct load_command lc;
        if (read_memory_native(pid, current_address, sizeof(struct load_command),
                               (unsigned char *)&lc) <= 0) {
            debug_log("错误：无法读取加载命令\n");
            return 0;
        }

        if (lc.cmd == LC_SEGMENT_64) {
            struct segment_command_64 seg;
            if (read_memory_native(pid, current_address, sizeof(struct segment_command_64),
                                   (unsigned char *)&seg) <= 0) {
                debug_log("错误：无法读取段命令\n");
                return 0;
            }
            image_size += seg.vmsize;
        }

        current_address += lc.cmdsize;
    }

    return image_size;
}

// 获取32位镜像大小
static uint64_t get_image_size_32(int pid, mach_vm_address_t base_address) {
    struct mach_header header;
    if (read_memory_native(pid, base_address, sizeof(struct mach_header),
                           (unsigned char *)&header) <= 0) {
        debug_log("错误：无法读取32位Mach-O头\n");
        return 0;
    }

    uint64_t image_size = 0;
    mach_vm_address_t current_address = base_address + sizeof(struct mach_header);

    for (int i = 0; i < header.ncmds; i++) {
        struct load_command lc;
        if (read_memory_native(pid, current_address, sizeof(struct load_command),
                               (unsigned char *)&lc) <= 0) {
            debug_log("错误：无法读取加载命令\n");
            return 0;
        }

        if (lc.cmd == LC_SEGMENT) {
            struct segment_command seg;
            if (read_memory_native(pid, current_address, sizeof(struct segment_command),
                                   (unsigned char *)&seg) <= 0) {
                debug_log("错误：无法读取段命令\n");
                return 0;
            }
            image_size += seg.vmsize;
        }

        current_address += lc.cmdsize;
    }

    return image_size;
}

// 获取模块大小
static uint64_t get_module_size(int pid, mach_vm_address_t address, int *is_64bit) {
    uint32_t magic;
    if (read_memory_native(pid, address, sizeof(uint32_t),
                           (unsigned char *)&magic) <= 0) {
        debug_log("错误：无法读取Mach-O魔数\n");
        return 0;
    }

    if (magic == MH_MAGIC_64) {
        *is_64bit = 1;
        return get_image_size_64(pid, address);
    } else if (magic == MH_MAGIC) {
        *is_64bit = 0;
        return get_image_size_32(pid, address);
    } else if (magic == FAT_MAGIC || magic == FAT_CIGAM) {
        struct fat_header fatHeader;
        if (read_memory_native(pid, address, sizeof(struct fat_header),
                               (unsigned char *)&fatHeader) <= 0) {
            debug_log("错误：无法读取FAT头\n");
            return 0;
        }

        struct fat_arch *archs = malloc(fatHeader.nfat_arch * sizeof(struct fat_arch));
        if (read_memory_native(pid, address + sizeof(struct fat_header),
                               fatHeader.nfat_arch * sizeof(struct fat_arch),
                               (unsigned char *)archs) <= 0) {
            debug_log("错误：无法读取FAT架构\n");
            free(archs);
            return 0;
        }

        for (int i = 0; i < fatHeader.nfat_arch; i++) {
            if (read_memory_native(pid, address + archs[i].offset, sizeof(uint32_t),
                                   (unsigned char *)&magic) <= 0) {
                debug_log("错误：无法在FAT二进制文件中读取Mach-O魔数\n");
                continue;
            }
            if (magic == MH_MAGIC_64) {
                *is_64bit = 1;
                uint64_t size = get_image_size_64(pid, address + archs[i].offset);
                free(archs);
                return size;
            } else if (magic == MH_MAGIC) {
                *is_64bit = 0;
                uint64_t size = get_image_size_32(pid, address + archs[i].offset);
                free(archs);
                return size;
            }
        }
        free(archs);
    }

    debug_log("错误：未知的Mach-O格式\n");
    return 0;
}

// 枚举模块
ModuleInfo *enummodule_native(pid_t pid, size_t *count) {
    task_t task;
    if (task_for_pid(mach_task_self(), pid, &task) != KERN_SUCCESS) {
        debug_log("错误：无法获取pid %d的任务\n", pid);
        *count = 0;
        return NULL;
    }

    struct task_dyld_info dyld_info;
    mach_msg_type_number_t count_info = TASK_DYLD_INFO_COUNT;

    if (task_info(task, TASK_DYLD_INFO, (task_info_t)&dyld_info, &count_info) != KERN_SUCCESS) {
        debug_log("错误：无法获取任务信息\n");
        *count = 0;
        return NULL;
    }

    struct dyld_all_image_infos all_image_infos;
    if (read_memory_native(pid, dyld_info.all_image_info_addr, sizeof(struct dyld_all_image_infos),
                           (unsigned char *)&all_image_infos) <= 0) {
        debug_log("错误：无法读取all_image_infos\n");
        *count = 0;
        return NULL;
    }

    struct dyld_image_info *image_infos = malloc(all_image_infos.infoArrayCount * sizeof(struct dyld_image_info));
    if (read_memory_native(pid, (mach_vm_address_t)all_image_infos.infoArray,
                           sizeof(struct dyld_image_info) * all_image_infos.infoArrayCount,
                           (unsigned char *)image_infos) <= 0) {
        debug_log("错误：无法读取image_infos\n");
        free(image_infos);
        *count = 0;
        return NULL;
    }

    ModuleInfo *moduleList = malloc(all_image_infos.infoArrayCount * sizeof(ModuleInfo));
    *count = 0;

    for (uint32_t i = 0; i < all_image_infos.infoArrayCount; i++) {
        char fpath[PATH_MAX];
        if (read_memory_native(pid, (mach_vm_address_t)image_infos[i].imageFilePath,
                               PATH_MAX, (unsigned char *)fpath) > 0) {
            ModuleInfo module;
            if (strlen(fpath) == 0 && proc_regionfilename != NULL) {
                char buffer[PATH_MAX];
                int ret = proc_regionfilename(pid, (uint64_t)image_infos[i].imageLoadAddress,
                                              buffer, sizeof(buffer));
                module.modulename = strdup(ret > 0 ? buffer : "None");
            } else {
                module.modulename = strdup(fpath);
            }

            module.base = (uintptr_t)image_infos[i].imageLoadAddress;
            module.size = (int)get_module_size(pid, (mach_vm_address_t)module.base, &module.is_64bit);

            moduleList[*count] = module;
            (*count)++;
        }
    }

    free(image_infos);
    return moduleList;
}

// 初始化函数
int native_init() {
    void *libsystem_kernel = dlopen("/usr/lib/system/libsystem_kernel.dylib", RTLD_NOW);
    if (!libsystem_kernel) {
        debug_log("错误：无法加载libsystem_kernel.dylib：%s\n", dlerror());
        return -1;
    }

    // 清除任何存在的错误
    dlerror();

    proc_regionfilename = (PROC_REGIONFILENAME)dlsym(libsystem_kernel, "proc_regionfilename");
    const char *dlsym_error = dlerror();
    if (dlsym_error) {
        debug_log("错误：无法加载proc_regionfilename符号：%s\n", dlsym_error);
        proc_regionfilename = NULL;
    }

    if (proc_regionfilename == NULL) {
        debug_log("警告：proc_regionfilename不可用。某些功能可能受限。\n");
    }
    return 1;
}