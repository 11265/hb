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
    pid_t 进程pid = std::stoi(argv[1]);
    std::cout << "目标进程PID: " << 进程pid << std::endl;

    rx_mem_scan scanner;

    // 尝试附加到目标进程
    std::cout << "正在尝试附加到进程..." << std::endl;
    if (!scanner.attach(进程pid)) {
        std::cout << "无法附加到进程 " << 进程pid << std::endl;
        return 1;
    }

    std::cout << "成功附加到进程 " << 进程pid << std::endl;

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
    std::vector<int> search_values = {100, 0x64, 0x64000000, 0x100, 0x1000000};
    for (int search_value : search_values) {
        std::cout << "开始搜索值 " << search_value << " (0x" << std::hex << search_value << std::dec << ") ..." << std::endl;
        result = scanner.search(&search_value, rx_compare_type_eq);
        std::cout << "搜索值 " << search_value << " 的结果: " << result.matched << " 个匹配" << std::endl;
        std::cout << "精确搜索耗时: " << result.time_used << " 毫秒, 内存使用: " << result.memory_used << " 字节" << std::endl;
    }


    std::cout << "扫描程序运行结束." << std::endl;
    return 0;
}