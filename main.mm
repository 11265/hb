#include <Foundation/Foundation.h>
#include <dlfcn.h>
#include <errno.h>
#include <mach-o/dyld_images.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach/mach.h>
#include <mach/task.h>
#include <mach/vm_map.h>
#include <mach/vm_region.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <iostream>
#include <string>
#include <vector>
#include <stdbool.h>  // 添加这行
#include <strings.h>
#include <cctype>  // 包含 std::tolower

// 其他必要的头文件
typedef struct
{
    int pid;
    const char *processname;
} ProcessInfo;

typedef struct
{
    uintptr_t base;
    int size;
    bool is_64bit;
    char *modulename;
} ModuleInfo;

extern "C" kern_return_t mach_vm_read_overwrite(vm_map_t, mach_vm_address_t, mach_vm_size_t,mach_vm_address_t, mach_vm_size_t *);

extern "C" kern_return_t mach_vm_write(vm_map_t, mach_vm_address_t, vm_offset_t,mach_msg_type_number_t);

extern "C" kern_return_t mach_vm_protect(vm_map_t, mach_vm_address_t, mach_vm_size_t, boolean_t,vm_prot_t);

extern "C" kern_return_t mach_vm_region(vm_map_t, mach_vm_address_t *, mach_vm_size_t *,vm_region_flavor_t, vm_region_info_t,mach_msg_type_number_t *, mach_port_t *);

typedef int (*PROC_REGIONFILENAME)(int pid, uint64_t address, void *buffer, uint32_t buffersize);
PROC_REGIONFILENAME proc_regionfilename = nullptr;

int debug_log(const char *format, ...)
{
    va_list list;
    va_start(list, format);
    NSString *originalFormatString = [NSString stringWithUTF8String:format];
    NSString *taggedFormatString =
        [NSString stringWithFormat:@"[MEMORYSERVER] %@", originalFormatString];

    NSLogv(taggedFormatString, list);
    va_end(list);
    return 0;
}// 调试日志函数

extern "C" pid_t get_pid_native()
{
    return getpid();
}// 获取当前进程ID的函数

extern "C" ssize_t read_memory_native(int pid, mach_vm_address_t address, mach_vm_size_t size,
                                      unsigned char *buffer)
{
    mach_port_t task;
    kern_return_t kr;
    if (pid == getpid())
    {
        task = mach_task_self();
    }
    else
    {
        kr = task_for_pid(mach_task_self(), pid, &task);
        if (kr != KERN_SUCCESS)
        {
            debug_log("Error: task_for_pid failed with error %d (%s)\n", kr, mach_error_string(kr));
            return -1;
        }
    }

    mach_vm_size_t out_size;
    kr = mach_vm_read_overwrite(task, address, size, (mach_vm_address_t)buffer, &out_size);
    if (kr != KERN_SUCCESS)
    {
        debug_log("Error: mach_vm_read_overwrite failed with error %d (%s)\n", kr,
                  mach_error_string(kr));
        return -1;
    }

    return static_cast<ssize_t>(out_size);
}// 从指定进程内存读取数据的函数

extern "C" ssize_t write_memory_native(int pid, mach_vm_address_t address, mach_vm_size_t size,
                                       unsigned char *buffer)
{
    task_t task;
    kern_return_t err;
    vm_prot_t original_protection;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object_name;
    bool is_embedded_mode = pid == getpid();

    if (is_embedded_mode)
    {
        task = mach_task_self();
    }
    else
    {
        err = task_for_pid(mach_task_self(), pid, &task);
        if (err != KERN_SUCCESS)
        {
            debug_log("Error: task_for_pid failed with error %d (%s)\n", err,
                      mach_error_string(err));
            return -1;
        }
    }

    if (!is_embedded_mode)
    {
        task_suspend(task);
    }

    mach_vm_address_t region_address = address;
    mach_vm_size_t region_size = size;
    // 获取当前内存保护
    err = mach_vm_region(task, &region_address, &region_size, VM_REGION_BASIC_INFO_64,
                         (vm_region_info_t)&info, &info_count, &object_name);
    if (err != KERN_SUCCESS)
    {
        debug_log("Error: mach_vm_region failed with error %d (%s) at address "
                  "0x%llx, size 0x%llx\n",
                  err, mach_error_string(err), address, size);
        if (!is_embedded_mode)
        {
            task_resume(task);
        }
        return -1;
    }
    original_protection = info.protection;

    // 修改内存保护以允许写入
    err = mach_vm_protect(task, address, size, false, VM_PROT_READ | VM_PROT_WRITE);
    if (err != KERN_SUCCESS)
    {
        debug_log("Error: mach_vm_protect (write enable) failed with error %d (%s)\n", err,
                  mach_error_string(err));
        if (!is_embedded_mode)
        {
            task_resume(task);
        }
        return -1;
    }

    // 写入内存
    err = mach_vm_write(task, address, (vm_offset_t)buffer, size);
    if (err != KERN_SUCCESS)
    {
        debug_log("Error: mach_vm_write failed with error %d (%s) at address "
                  "0x%llx, size 0x%llx\n",
                  err, mach_error_string(err), address, size);
        mach_vm_protect(task, address, size, false,
                        original_protection);  // Attempt to restore protection
        if (!is_embedded_mode)
        {
            task_resume(task);
        }
        return -1;
    }

    // 重置内存保护
    err = mach_vm_protect(task, address, size, false, original_protection);
    if (err != KERN_SUCCESS)
    {
        debug_log("Warning: mach_vm_protect (restore protection) failed with error "
                  "%d (%s)\n",
                  err, mach_error_string(err));
        if (!is_embedded_mode)
        {
            task_resume(task);
        }
        return -1;
    }

    if (!is_embedded_mode)
    {
        task_resume(task);
    }
    return static_cast<ssize_t>(size);
}// 向指定进程内存写入数据的函数

extern "C" void enumerate_regions_to_buffer(pid_t pid, char *buffer, size_t buffer_size)
{
    task_t task;
    kern_return_t err;
    vm_address_t address = 0;
    vm_size_t size = 0;
    natural_t depth = 1;

    if (pid == getpid())
    {
        task = mach_task_self();
    }
    else
    {
        err = task_for_pid(mach_task_self(), pid, &task);
        if (err != KERN_SUCCESS)
        {
            debug_log("Error: task_for_pid failed with error %d (%s)\n", err,
                      mach_error_string(err));
            snprintf(buffer, buffer_size, "Failed to get task for pid %d\n", pid);
            return;
        }
    }

    size_t pos = 0;
    while (true)
    {
        vm_region_submap_info_data_64_t info;
        mach_msg_type_number_t info_count = VM_REGION_SUBMAP_INFO_COUNT_64;

        if (vm_region_recurse_64(task, &address, &size, &depth, (vm_region_info_t)&info,
                                 &info_count) != KERN_SUCCESS)
        {
            break;
        }

        if (info.is_submap)
        {
            depth++;
        }
        else
        {
            char protection[4] = "---";
            if (info.protection & VM_PROT_READ) protection[0] = 'r';
            if (info.protection & VM_PROT_WRITE) protection[1] = 'w';
            if (info.protection & VM_PROT_EXECUTE) protection[2] = 'x';

            pos += snprintf(buffer + pos, buffer_size - pos, "%llx-%llx %s\n",
                            static_cast<unsigned long long>(address),
                            static_cast<unsigned long long>(address + size), protection);

            if (pos >= buffer_size - 1) break;

            address += size;
        }
    }
}// 枚举指定进程的内存区域并将其写入缓冲区的函数

extern "C" ProcessInfo *enumprocess_native(size_t *count)
{
    int err;
    struct kinfo_proc *result;
    bool done;
    static const int name[] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    size_t length;

    result = nullptr;
    done = false;

    do
    {
        length = 0;
        err = sysctl(const_cast<int *>(name), (sizeof(name) / sizeof(*name)) - 1, nullptr, &length,
                     nullptr, 0);
        if (err == -1)
        {
            err = errno;
        }

        if (err == 0)
        {
            result = static_cast<struct kinfo_proc *>(malloc(length));
            if (result == nullptr)
            {
                err = ENOMEM;
            }
        }

        if (err == 0)
        {
            err = sysctl(const_cast<int *>(name), (sizeof(name) / sizeof(*name)) - 1, result,
                         &length, nullptr, 0);
            if (err == -1)
            {
                err = errno;
            }
            if (err == 0)
            {
                done = true;
            }
            else if (err == ENOMEM)
            {
                free(result);
                result = nullptr;
                err = 0;
            }
        }
    } while (err == 0 && !done);

    if (err == 0 && result != nullptr)
    {
        *count = length / sizeof(struct kinfo_proc);
        ProcessInfo *processes = static_cast<ProcessInfo *>(malloc(*count * sizeof(ProcessInfo)));

        for (size_t i = 0; i < *count; i++)
        {
            processes[i].pid = result[i].kp_proc.p_pid;
            processes[i].processname = strdup(result[i].kp_proc.p_comm);
        }

        free(result);
        return processes;
    }
    else
    {
        if (result != nullptr)
        {
            free(result);
        }
        debug_log("Error: Failed to enumerate processes, error %d\n", err);
        return nullptr;
    }
}// 枚举所有进程并返回进程信息的函数

extern "C" bool suspend_process(pid_t pid)
{
    task_t task;
    kern_return_t err;
    bool is_embedded_mode = pid == getpid();
    if (is_embedded_mode)
    {
        debug_log("Error: Cannot suspend self process\n");
        return false;
    }
    err = task_for_pid(mach_task_self(), pid, &task);
    if (err != KERN_SUCCESS)
    {
        debug_log("Error: task_for_pid failed with error %d (%s)\n", err, mach_error_string(err));
        return false;
    }
    err = task_suspend(task);
    if (err != KERN_SUCCESS)
    {
        debug_log("Error: task_suspend failed with error %d (%s)\n", err, mach_error_string(err));
        return false;
    }

    return true;
}// 挂起指定进程的函数

extern "C" bool resume_process(pid_t pid)
{
    task_t task;
    kern_return_t err;
    bool is_embedded_mode = pid == getpid();
    if (is_embedded_mode)
    {
        debug_log("Error: Cannot resume self process\n");
        return false;
    }
    err = task_for_pid(mach_task_self(), pid, &task);
    if (err != KERN_SUCCESS)
    {
        debug_log("Error: task_for_pid failed with error %d (%s)\n", err, mach_error_string(err));
        return false;
    }
    err = task_resume(task);
    if (err != KERN_SUCCESS)
    {
        debug_log("Error: task_resume failed with error %d (%s)\n", err, mach_error_string(err));
        return false;
    }

    return true;
}// 恢复指定进程的函数
//-----------------------------------------------------------------------------------------------------------------------------------------
static std::uint64_t get_image_size_64(int pid, mach_vm_address_t base_address)
{
    mach_header_64 header;
    if (read_memory_native(pid, base_address, sizeof(mach_header_64),
                           reinterpret_cast<unsigned char *>(&header)) <= 0)
    {
        debug_log("Error: Failed to read 64-bit Mach-O header\n");
        return 0;
    }

    std::uint64_t image_size = 0;
    mach_vm_address_t current_address = base_address + sizeof(mach_header_64);

    for (int i = 0; i < header.ncmds; i++)
    {
        load_command lc;
        if (read_memory_native(pid, current_address, sizeof(load_command),
                               reinterpret_cast<unsigned char *>(&lc)) <= 0)
        {
            debug_log("Error: Failed to read load command\n");
            return 0;
        }

        if (lc.cmd == LC_SEGMENT_64)
        {
            segment_command_64 seg;
            if (read_memory_native(pid, current_address, sizeof(segment_command_64),
                                   reinterpret_cast<unsigned char *>(&seg)) <= 0)
            {
                debug_log("Error: Failed to read segment command\n");
                return 0;
            }
            image_size += seg.vmsize;
        }

        current_address += lc.cmdsize;
    }

    return image_size;
}// 获取64位Mach-O镜像大小的函数

static std::uint64_t get_image_size_32(int pid, mach_vm_address_t base_address)
{
    mach_header header;
    if (read_memory_native(pid, base_address, sizeof(mach_header),
                           reinterpret_cast<unsigned char *>(&header)) <= 0)
    {
        debug_log("Error: Failed to read 32-bit Mach-O header\n");
        return 0;
    }

    std::uint64_t image_size = 0;
    mach_vm_address_t current_address = base_address + sizeof(mach_header);

    for (int i = 0; i < header.ncmds; i++)
    {
        load_command lc;
        if (read_memory_native(pid, current_address, sizeof(load_command),
                               reinterpret_cast<unsigned char *>(&lc)) <= 0)
        {
            debug_log("Error: Failed to read load command\n");
            return 0;
        }

        if (lc.cmd == LC_SEGMENT)
        {
            segment_command seg;
            if (read_memory_native(pid, current_address, sizeof(segment_command),
                                   reinterpret_cast<unsigned char *>(&seg)) <= 0)
            {
                debug_log("Error: Failed to read segment command\n");
                return 0;
            }
            image_size += seg.vmsize;
        }

        current_address += lc.cmdsize;
    }

    return image_size;
}// 获取32位Mach-O镜像大小的函数

static std::uint64_t get_module_size(int pid, mach_vm_address_t address, bool *is_64bit)
{
    std::uint32_t magic;
    // 从指定地址读取 Mach-O 魔数
    if (read_memory_native(pid, address, sizeof(std::uint32_t),
                           reinterpret_cast<unsigned char *>(&magic)) <= 0)
    {
        debug_log("错误：读取 Mach-O 魔数失败\n");
        return 0;
    }

    // 检查魔数以确定文件格式
    if (magic == MH_MAGIC_64)
    {
        *is_64bit = true;
        // 如果是 64 位 Mach-O 文件，获取其大小
        return get_image_size_64(pid, address);
    }
    else if (magic == MH_MAGIC)
    {
        *is_64bit = false;
        // 如果是 32 位 Mach-O 文件，获取其大小
        return get_image_size_32(pid, address);
    }
    else if (magic == FAT_MAGIC || magic == FAT_CIGAM)
    {
        fat_header fatHeader;
        // 读取 FAT 文件头
        if (read_memory_native(pid, address, sizeof(fat_header),
                               reinterpret_cast<unsigned char *>(&fatHeader)) <= 0)
        {
            debug_log("错误：读取 FAT 文件头失败\n");
            return 0;
        }

        std::vector<fat_arch> archs(fatHeader.nfat_arch);
        // 读取 FAT 文件的所有架构
        if (read_memory_native(pid, address + sizeof(fat_header),
                               fatHeader.nfat_arch * sizeof(fat_arch),
                               reinterpret_cast<unsigned char *>(archs.data())) <= 0)
        {
            debug_log("错误：读取 FAT 架构失败\n");
            return 0;
        }

        // 遍历所有架构，检查每个架构的魔数
        for (const auto &arch : archs)
        {
            if (read_memory_native(pid, address + arch.offset, sizeof(std::uint32_t),
                                   reinterpret_cast<unsigned char *>(&magic)) <= 0)
            {
                debug_log("错误：读取 FAT 二进制中的 Mach-O 魔数失败\n");
                continue;
            }
            if (magic == MH_MAGIC_64)
            {
                *is_64bit = true;
                // 如果是 64 位 Mach-O 文件，获取其大小
                return get_image_size_64(pid, address + arch.offset);
            }
            else if (magic == MH_MAGIC)
            {
                *is_64bit = false;
                // 如果是 32 位 Mach-O 文件，获取其大小
                return get_image_size_32(pid, address + arch.offset);
            }
        }
    }

    // 如果无法识别文件格式，返回 0
    debug_log("错误：未知的 Mach-O 格式\n");
    return 0;
}
//----------------------------------------------------------------------------------------------------

//用于获取指定进程的模块信息
extern "C" ModuleInfo *enummodule_native(pid_t pid, size_t *count)
{
    task_t task;
    // 尝试获取指定进程的任务端口
    if (task_for_pid(mach_task_self(), pid, &task) != KERN_SUCCESS)
    {
        debug_log("错误：获取 pid %d 的任务失败\n", pid);
        *count = 0;
        return nullptr;
    }

    task_dyld_info dyld_info;
    mach_msg_type_number_t count_info = TASK_DYLD_INFO_COUNT;

    // 获取任务的动态库信息
    if (task_info(task, TASK_DYLD_INFO, reinterpret_cast<task_info_t>(&dyld_info), &count_info) != KERN_SUCCESS)
    {
        debug_log("错误：获取任务信息失败\n");
        *count = 0;
        return nullptr;
    }

    dyld_all_image_infos all_image_infos;
    // 读取所有图像信息
    if (read_memory_native(pid, dyld_info.all_image_info_addr, sizeof(dyld_all_image_infos),
                           reinterpret_cast<unsigned char *>(&all_image_infos)) <= 0)
    {
        debug_log("错误：读取 all_image_infos 失败\n");
        *count = 0;
        return nullptr;
    }

    std::vector<dyld_image_info> image_infos(all_image_infos.infoArrayCount);
    // 读取图像信息数组
    if (read_memory_native(pid, reinterpret_cast<mach_vm_address_t>(all_image_infos.infoArray),
                           sizeof(dyld_image_info) * all_image_infos.infoArrayCount,
                           reinterpret_cast<unsigned char *>(image_infos.data())) <= 0)
    {
        debug_log("错误：读取 image_infos 失败\n");
        *count = 0;
        return nullptr;
    }

    std::vector<ModuleInfo> moduleList;
    moduleList.reserve(all_image_infos.infoArrayCount);

    // 遍历所有图像信息
    for (const auto &info : image_infos)
    {
        char fpath[PATH_MAX];
        // 读取模块文件路径
        if (read_memory_native(pid, reinterpret_cast<mach_vm_address_t>(info.imageFilePath),
                               PATH_MAX, reinterpret_cast<unsigned char *>(fpath)) > 0)
        {
            ModuleInfo module;
            if (strlen(fpath) == 0 && proc_regionfilename != nullptr)
            {
                char buffer[PATH_MAX];
                // 如果路径为空，尝试获取区域文件名
                int ret =
                    proc_regionfilename(pid, reinterpret_cast<std::uint64_t>(info.imageLoadAddress),
                                        buffer, sizeof(buffer));
                module.modulename = strdup(ret > 0 ? buffer : "None");
            }
            else
            {
                module.modulename = strdup(fpath);
            }

            module.base = reinterpret_cast<std::uintptr_t>(info.imageLoadAddress);
            module.size = static_cast<std::int32_t>(get_module_size(
                pid, static_cast<mach_vm_address_t>(module.base), &module.is_64bit));

            moduleList.push_back(module);
        }
    }

    *count = moduleList.size();
    // 分配内存并复制模块信息到结果数组
    ModuleInfo *result = static_cast<ModuleInfo *>(malloc(*count * sizeof(ModuleInfo)));
    std::copy(moduleList.begin(), moduleList.end(), result);

    return result;
}


extern "C" int native_init()
{
    void *libsystem_kernel = dlopen("/usr/lib/system/libsystem_kernel.dylib", RTLD_NOW);
    if (!libsystem_kernel)
    {
        debug_log("Error: Failed to load libsystem_kernel.dylib: %s\n", dlerror());
        return -1;
    }

    // Clear any existing error
    dlerror();

    proc_regionfilename = (PROC_REGIONFILENAME)dlsym(libsystem_kernel, "proc_regionfilename");
    const char *dlsym_error = dlerror();
    if (dlsym_error)
    {
        debug_log("Error: Failed to load proc_regionfilename symbol: %s\n", dlsym_error);
        proc_regionfilename = nullptr;
    }

    if (proc_regionfilename == nullptr)
    {
        debug_log("Warning: proc_regionfilename is not available. Some "
                  "functionality may be limited.\n");
    }
    return 1;
}

// 获取进程列表
extern "C" int get_proc_list(kinfo_proc **procList, size_t *procCount) 
{
    int err;
    kinfo_proc *result = NULL;
    bool done = false;
    static const int name[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
    size_t length;

    *procCount = 0;

    do {
        length = 0;
        err = sysctl((int *)name, (sizeof(name) / sizeof(*name)) - 1, NULL, &length, NULL, 0);
        if (err == -1) {
            err = errno;
            break;
        }

        result = (kinfo_proc *)malloc(length);
        if (result == NULL) {
            err = ENOMEM;
            break;
        }

        err = sysctl((int *)name, (sizeof(name) / sizeof(*name)) - 1, result, &length, NULL, 0);
        if (err == -1) {
            err = errno;
            free(result);
            result = NULL;
        } else {
            done = true;
        }
    } while (err == 0 && !done);

    if (err != 0 && result != NULL) {
        free(result);
        result = NULL;
    }

    if (err == 0) {
        *procList = result;
        *procCount = length / sizeof(kinfo_proc);
    }

    return err;
}

// 根据进程名获取PID
extern "C"  pid_t get_pid_by_name(const char *process_name) 
{
    kinfo_proc *procList = NULL;
    size_t procCount = 0;
    int err = get_proc_list(&procList, &procCount);
    
    if (err != 0) {
        fprintf(stderr, "无法获取进程列表: %d\n", err);
        return -1;
    }

    pid_t target_pid = -1;
    for (size_t i = 0; i < procCount; i++) {
        if (strcmp(procList[i].kp_proc.p_comm, process_name) == 0) {
            target_pid = procList[i].kp_proc.p_pid;
            break;
        }
    }

    free(procList);
    return target_pid;
}

// 不区分大小写的字符串比较
extern "C" bool my_strcasecmp(const char *s1, const char *s2) {
    while (*s1 && *s2 && std::tolower(static_cast<unsigned char>(*s1)) == std::tolower(static_cast<unsigned char>(*s2))) {
        ++s1;
        ++s2;
    }
    return (std::tolower(static_cast<unsigned char>(*s1)) == std::tolower(static_cast<unsigned char>(*s2)));
}
// 查找指定模块名称的函数
extern "C" uintptr_t find_module_base(pid_t pid, const char *module_name) {
    size_t module_count;
    ModuleInfo *modules = enummodule_native(pid, &module_count);
    
    if (modules == nullptr) {
        debug_log("无法获取模块信息\n");
        return 0;  // 或者返回一个特定的错误标记
    }

    uintptr_t base_address = 0;
    for (size_t i = 0; i < module_count; ++i) {
        if (my_strcasecmp(modules[i].modulename, module_name) == 0) {
            base_address = modules[i].base;
            break;
        }
    }

    // 释放模块信息中的模块名称内存
    for (size_t i = 0; i < module_count; ++i) {
        free(modules[i].modulename);
    }

    // 释放分配的内存
    free(modules);

    return base_address;
}

//--------------------------------------------------
#define TARGET_PROCESS_NAME "pvz"
    //pid_t target_pid = 12345;  // 替换为实际的目标进程 ID
    mach_vm_address_t address = 0x1000;  // 替换为实际的内存地址
    mach_vm_size_t size = sizeof(int);  // 要读取的字节数（对于 i32 类型）
    unsigned char buffer[sizeof(int)];  // 确保缓冲区足够大以存储读取的数据

extern "C" int c_main() 
{
    pid_t target_pid = get_pid_by_name(TARGET_PROCESS_NAME);
    if (target_pid == -1) 
    {
        debug_log("未找到进程：%s\n", TARGET_PROCESS_NAME);
        return -1;
    }
    debug_log("找到进程: %s，PID: %d\n", TARGET_PROCESS_NAME, target_pid);
    
    size_t module_count;
    ModuleInfo *modules = enummodule_native(target_pid, &module_count);

    if (modules == nullptr) 
    {
        debug_log("无法获取模块信息\n");
        return -1;
    }
    debug_log("模块数量: %zu\n", module_count);
/*
    for (size_t i = 0; i < module_count; ++i) 
    {
        debug_log("模块名称: %s\n", modules[i].modulename);  // 直接打印 char *
        debug_log("基地址: 0x%lx\n", modules[i].base);
        debug_log("大小: %d bytes\n", modules[i].size);
        debug_log("64位: %s\n", modules[i].is_64bit ? "是" : "否");
    }
*/
/*
    // 打印所有模块名称以进行调试
    for (size_t i = 0; i < module_count; ++i) 
    {
        debug_log("模块名称: %s\n", modules[i].modulename);
    }
*/
    // 查找模块基地址
    const char *module_name = "pvz";
    uintptr_t base_address = find_module_base(target_pid, module_name);
    
    if (base_address == 0) {
        debug_log("未找到模块：%s\n", module_name);
    } else {
        debug_log("模块 %s 的基地址: 0x%lx\n", module_name, base_address);
    }

    ssize_t bytes_read = read_memory_native(target_pid, address, size, buffer);

    if (bytes_read == sizeof(int)) {
        // 将 buffer 转换为 i32
        int value;
        memcpy(&value, buffer, sizeof(int));
        
        std::cout << "Read i32 value: " << value << std::endl;
    } else {
        std::cerr << "Failed to read memory or read size mismatch" << std::endl;
    }

    // 释放分配的内存
    free(modules);
    debug_log("运行结束.\n");

    return 0;
}