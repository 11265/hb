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

#include "rx_mem_scan.h"
#include <iostream>
#include <string>

extern "C" int c_main(int argc, char* argv[]) 
{
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <pid>" << std::endl;
        return 1;
    }

    pid_t target_pid = std::stoi(argv[1]);
    rx_mem_scan scanner;

    // 附加到目标进程
    if (!scanner.attach(target_pid)) {
        std::cerr << "Failed to attach to process " << target_pid << std::endl;
        return 1;
    }

    std::cout << "Successfully attached to process " << target_pid << std::endl;

    // 设置搜索类型为整数
    rx_search_int_type int_type;
    scanner.set_search_value_type(&int_type);

    // 执行首次模糊搜索
    search_result_t result = scanner.first_fuzzy_search();
    std::cout << "First fuzzy search result: " << result.matched << " matches" << std::endl;

    // 执行具体值搜索
    int search_value = 100;  // 假设我们要搜索值为100的整数
    result = scanner.search(&search_value, rx_compare_type_eq);
    std::cout << "Search for value 100 result: " << result.matched << " matches" << std::endl;

    // 获取匹配结果的第一页
    rx_memory_page_pt page = scanner.page_of_matched(0, 10);  // 获取前10个结果
    if (page && page->addresses) {
        std::cout << "First 10 matching addresses:" << std::endl;
        for (size_t i = 0; i < page->addresses->size(); ++i) {
            std::cout << std::hex << "0x" << (*page->addresses)[i] << std::dec << std::endl;
        }
    }

    // 修改第一个匹配的值
    if (page && page->addresses && !page->addresses->empty()) {
        int new_value = 200;
        vm_address_t address = (*page->addresses)[0];
        kern_return_t result = scanner.write_val(address, &new_value);
        if (result == KERN_SUCCESS) {
            std::cout << "Successfully wrote value 200 to address 0x" << std::hex << address << std::dec << std::endl;
        } else {
            std::cerr << "Failed to write value" << std::endl;
        }
    }

    delete page;  // 清理内存

    // 执行字符串搜索
    scanner.search_str("Hello, World!");
    debug_log("运行结束.\n");
    return 0;
}


