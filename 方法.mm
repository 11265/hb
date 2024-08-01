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
/*
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
*/
/*
    //跨进程读取内存
    int32_t value;

    ssize_t bytes_read = read_memory_native(target_pid, target_address, sizeof(int32_t), reinterpret_cast<unsigned char*>(&value));
    if (bytes_read == sizeof(int32_t)) {
            debug_log("读i32值: %d\n", value);
    } else {
            debug_log("读取内存失败或读取大小不匹配，读取字节数: %zd\n", bytes_read);
    }
*/
//--------------------------------------------
/*
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
*/
//--------------------------------------------------