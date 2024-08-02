#include <Foundation/Foundation.h>
#include <iostream>
#include <string>
#include "rx_mem_scan.h"

extern "C" int c_main(int argc, char* argv[]) 
{
    // 检查命令行参数
    if (argc != 2) {
        std::cout << "用法: " << argv[0] << " <目标进程PID>" << std::endl;
        return 1;
    }

    // 获取目标进程PID
    pid_t target_pid = std::stoi(argv[1]);
    std::cout << "目标进程PID: " << target_pid << std::endl;

    rx_mem_scan scanner;

    // 尝试附加到目标进程
    std::cout << "正在尝试附加到进程..." << std::endl;
    if (!scanner.attach(target_pid)) {
        std::cout << "无法附加到进程 " << target_pid << std::endl;
        return 1;
    }

    std::cout << "成功附加到进程 " << target_pid << std::endl;

    // 设置搜索值类型为整数
    rx_search_int_type int_type;
    scanner.set_search_value_type(&int_type);
    std::cout << "搜索值类型设置为整数" << std::endl;

    // 执行首次模糊搜索
    std::cout << "开始执行首次模糊搜索..." << std::endl;
    search_result_t result = scanner.first_fuzzy_search();
    std::cout << "首次模糊搜索结果: " << result.matched << " 个匹配" << std::endl;
    std::cout << "模糊搜索耗时: " << result.time_used << " 毫秒, 内存使用: " << result.memory_used << " 字节" << std::endl;

    // 搜索特定值
    int search_value = 100;
    std::cout << "开始搜索值 " << search_value << " ..." << std::endl;
    result = scanner.search(&search_value, rx_compare_type_eq);
    std::cout << "搜索值 100 的结果: " << result.matched << " 个匹配" << std::endl;
    std::cout << "精确搜索耗时: " << result.time_used << " 毫秒, 内存使用: " << result.memory_used << " 字节" << std::endl;

    // 如果没有找到匹配，尝试搜索其他值
    if (result.matched == 0) {
        search_value = 0;  // 尝试搜索0
        std::cout << "尝试搜索值 " << search_value << " ..." << std::endl;
        result = scanner.search(&search_value, rx_compare_type_eq);
        std::cout << "搜索值 0 的结果: " << result.matched << " 个匹配" << std::endl;
    }

    // 获取匹配结果的前10个地址
    if (result.matched > 0) {
        rx_memory_page_pt page = scanner.page_of_matched(0, 10);
        if (page && page->addresses) {
            std::cout << "前10个匹配地址:" << std::endl;
            for (size_t i = 0; i < page->addresses->size(); ++i) {
                std::cout << "0x" << std::hex << (*page->addresses)[i] << std::dec << std::endl;
            }

            // 尝试修改第一个匹配地址的值
            if (!page->addresses->empty()) {
                int new_value = 200;
                vm_address_t address = (*page->addresses)[0];
                std::cout << "尝试将地址 0x" << std::hex << address << std::dec << " 的值修改为 " << new_value << std::endl;
                kern_return_t write_result = scanner.write_val(address, &new_value);
                if (write_result == KERN_SUCCESS) {
                    std::cout << "成功将值 200 写入地址 0x" << std::hex << address << std::dec << std::endl;
                } else {
                    std::cout << "写入值失败, 错误码: " << write_result << std::endl;
                }
            }
        }
        delete page;
    } else {
        std::cout << "没有找到匹配的地址" << std::endl;
    }

    // 执行字符串搜索
    std::string search_str = "Hello, World!";
    std::cout << "开始搜索字符串: \"" << search_str << "\"" << std::endl;
    scanner.search_str(search_str);

    std::cout << "扫描程序运行结束." << std::endl;
    return 0;
}