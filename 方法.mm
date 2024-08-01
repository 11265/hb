
    for (size_t i = 0; i < module_count; ++i) 
    {
        debug_log("模块名称: %s\n", modules[i].modulename);  // 直接打印 char *
        debug_log("基地址: 0x%lx\n", modules[i].base);
        debug_log("大小: %d bytes\n", modules[i].size);
        debug_log("64位: %s\n", modules[i].is_64bit ? "是" : "否");
    }


    // 打印所有模块名称以进行调试
    for (size_t i = 0; i < module_count; ++i) 
    {
        debug_log("模块名称: %s\n", modules[i].modulename);
    }


    //跨进程读取内存
    int32_t value;
    while (true) 
    {
        ssize_t bytes_read = read_memory_native(target_pid, target_address, sizeof(int32_t), reinterpret_cast<unsigned char*>(&value));
        if (bytes_read == sizeof(int32_t)) {
            debug_log("读i32值: %d\n", value);
        } else {
            debug_log("读取内存失败或读取大小不匹配，读取字节数: %zd\n", bytes_read);
        }
        
        // 每秒读取60次，即每次读取后等待大约16.666毫秒
        usleep(1000000 / 60);  // 16,666微秒
    }


    //跨进程读取内存
    int32_t value;

    ssize_t bytes_read = read_memory_native(target_pid, target_address, sizeof(int32_t), reinterpret_cast<unsigned char*>(&value));
    if (bytes_read == sizeof(int32_t)) {
            debug_log("读i32值: %d\n", value);
    } else {
            debug_log("读取内存失败或读取大小不匹配，读取字节数: %zd\n", bytes_read);
    }

//--------------------------------------------

    // 要写入的数据
    int32_t new_value = 42;
    unsigned char *buffer = reinterpret_cast<unsigned char*>(&new_value);

    // 调用函数写入数据
    ssize_t bytes_written = write_memory_native(target_pid, target_address, sizeof(new_value), buffer);
    if (bytes_written == sizeof(new_value)) {
        debug_log("成功写入 i32 值: %d\n", new_value);
    } else {
        debug_log("写入内存失败，写入字节数: %zd\n", bytes_written);
    }

//--------------------------------------------------

#define TARGET_PROCESS_NAME "pvz"

mach_vm_address_t target_address = 0x1060E1388; // 基地址
mach_vm_address_t 偏移1 = 0x20A7AA0; // 第一级偏移
mach_vm_address_t 偏移2 = 0x400; // 第二级偏移

extern "C" int c_main() 
{   
    // 查找进程 PID
    pid_t target_pid = get_pid_by_name(TARGET_PROCESS_NAME);
    if (target_pid == -1) 
    {
        debug_log("未找到进程：%s\n", TARGET_PROCESS_NAME);
        return -1;
    }
    debug_log("找到进程: %s，PID: %d\n", TARGET_PROCESS_NAME, target_pid);
    
    // 遍历进程模块
    size_t module_count;
    ModuleInfo *modules = enummodule_native(target_pid, &module_count);
    if (modules == nullptr) 
    {
        debug_log("无法获取模块信息\n");
        return -1;
    }
    debug_log("模块数量: %zu\n", module_count);

    // 查找模块基地址
    const char *module_name = "pvz";
    uintptr_t base_address = find_module_base(target_pid, module_name);
    if (base_address == 0) {
        debug_log("未找到模块：%s\n", module_name);
        free(modules);
        return -1;
    }
    //debug_log("模块名称: %s 的基地址: 0x%lx\n", module_name, base_address);
    debug_log("模块名称: %s\n", module_name);
    debug_log("模块基址: 0x%zd\n", base_address);

    // 读取第一级指针
    uintptr_t first_pointer;
    ssize_t bytes_read = read_memory_native(target_pid, base_address + 偏移1, sizeof(uintptr_t), reinterpret_cast<unsigned char*>(&first_pointer));
    if (bytes_read != sizeof(uintptr_t)) {
        debug_log("读取第一级指针失败，读取字节数: %zd\n", bytes_read);
        free(modules);
        return -1;
    }
    debug_log("第一级指针: %zu\n", first_pointer);

    // 读取第二级指针
    uintptr_t second_pointer;
    bytes_read = read_memory_native(target_pid, first_pointer + 偏移2, sizeof(uintptr_t), reinterpret_cast<unsigned char*>(&second_pointer));
    if (bytes_read != sizeof(uintptr_t)) {
        debug_log("读取第二级指针失败，读取字节数: %zd\n", bytes_read);
        free(modules);
        return -1;
    }
    debug_log("第二级指针: %zu\n", second_pointer);


    // 释放分配的内存
    free(modules);
    debug_log("运行结束.\n");

    return 0;
}
//--------------------------------------------