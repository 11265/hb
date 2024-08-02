#include <Foundation/Foundation.h>
#include <iostream>
#include <string>
#include "rx_mem_scan.h"

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
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <pid>" << std::endl;
        return 1;
    }

    pid_t target_pid = std::stoi(argv[1]);
    rx_mem_scan scanner;

    if (!scanner.attach(target_pid)) {
        std::cerr << "Failed to attach to process " << target_pid << std::endl;
        return 1;
    }

    std::cout << "Successfully attached to process " << target_pid << std::endl;

    rx_search_int_type int_type;
    scanner.set_search_value_type(&int_type);

    search_result_t result = scanner.first_fuzzy_search();
    std::cout << "First fuzzy search result: " << result.matched << " matches" << std::endl;

    int search_value = 100;
    result = scanner.search(&search_value, rx_compare_type_eq);
    std::cout << "Search for value 100 result: " << result.matched << " matches" << std::endl;

    rx_memory_page_pt page = scanner.page_of_matched(0, 10);
    if (page && page->addresses) {
        std::cout << "First 10 matching addresses:" << std::endl;
        for (size_t i = 0; i < page->addresses->size(); ++i) {
            std::cout << std::hex << "0x" << (*page->addresses)[i] << std::dec << std::endl;
        }
    }

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

    delete page;

    scanner.search_str("Hello, World!");
    debug_log("运行结束.\n");
    return 0;
}