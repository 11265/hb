#include <Foundation/Foundation.h>
#include <iostream>
#include <string>
#include "rx_mem_scan.h"

// 调试日志函数
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
}

extern "C" int c_main(int argc, char* argv[]) 
{
    // 检查命令行参数
    if (argc != 2) {
        std::cout << "用法: " << argv[0] << " <目标进程PID>" << std::endl;
        return 1;
    }

    // 获取目标进程PID
    pid_t target_pid = std::stoi(argv[1]);
    debug_log("目标进程PID: %d", target_pid);

    rx_mem_scan scanner;

    // 尝试附加到目标进程
    debug_log("正在尝试附加到进程...");
    if (!scanner.attach(target_pid)) {
        std::cerr << "无法附加到进程 " << target_pid << std::endl;
        return 1;
    }

    std::cout << "成功附加到进程 " << target_pid << std::endl;

    // 设置搜索值类型为整数
    rx_search_int_type int_type;
    scanner.set_search_value_type(&int_type);
    debug_log("搜索值类型设置为整数");

    // 执行首次模糊搜索
    debug_log("开始执行首次模糊搜索...");
    search_result_t result = scanner.first_fuzzy_search();
    std::cout << "首次模糊搜索结果: " << result.matched << " 个匹配" << std::endl;
    debug_log("模糊搜索耗时: %u 毫秒, 内存使用: %u 字节", result.time_used, result.memory_used);

    // 搜索特定值
    int search_value = 100;
    debug_log("开始搜索值 %d ...", search_value);
    result = scanner.search(&search_value, rx_compare_type_eq);
    std::cout << "搜索值 100 的结果: " << result.matched << " 个匹配" << std::endl;
    debug_log("精确搜索耗时: %u 毫秒, 内存使用: %u 字节", result.time_used, result.memory_used);

    // 获取匹配结果的前10个地址
    rx_memory_page_pt page = scanner.page_of_matched(0, 10);
    if (page && page->addresses) {
        std::cout << "前10个匹配地址:" << std::endl;
        for (size_t i = 0; i < page->addresses->size(); ++i) {
            std::cout << std::hex << "0x" << (*page->addresses)[i] << std::dec << std::endl;
        }
    } else {
        debug_log("未找到匹配地址");
    }

    // 尝试修改第一个匹配地址的值
    if (page && page->addresses && !page->addresses->empty()) {
        int new_value = 200;
        vm_address_t address = (*page->addresses)[0];
        debug_log("尝试将地址 0x%llx 的值修改为 %d", address, new_value);
        kern_return_t result = scanner.write_val(address, &new_value);
        if (result == KERN_SUCCESS) {
            std::cout << "成功将值 200 写入地址 0x" << std::hex << address << std::dec << std::endl;
        } else {
            std::cerr << "写入值失败" << std::endl;
            debug_log("写入失败, 错误码: %d", result);
        }
    } else {
        debug_log("没有可修改的地址");
    }

    delete page;

    // 执行字符串搜索
    std::string search_str = "Hello, World!";
    debug_log("开始搜索字符串: \"%s\"", search_str.c_str());
    scanner.search_str(search_str);

    debug_log("扫描程序运行结束.\n");
    return 0;
}