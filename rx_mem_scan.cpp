#include "rx_mem_scan.h"
#include "lz4/lz4.h"
#include <sys/time.h>
// 在文件开头添加
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <cctype>

static long get_timestamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

rx_mem_scan::rx_mem_scan() {
    _idle                   = true;
    _regions_p              = NULL;
    _search_value_type_p    = NULL;
    _last_search_val_p      = NULL;
}

rx_mem_scan::~rx_mem_scan() {
    free_memory();
}

void rx_mem_scan::free_memory() {
    free_regions();
    if (_last_search_val_p) {
        free(_last_search_val_p);
        _last_search_val_p = NULL;
    }
}

void rx_mem_scan::reset() {
    free_memory();
    init_regions();
    _idle = true;
}

boolean_t rx_mem_scan::attach(pid_t pid) {
    _target_pid = pid;
    kern_return_t ret = task_for_pid(mach_task_self(), pid, &_target_task);
    if (ret != KERN_SUCCESS) {
        _trace("Attach to: %d Failed: %d %s\n", pid, ret, mach_error_string(ret));
        return false;
    }
    reset();
    return true;
}

pid_t rx_mem_scan::target_pid() {
    return _target_pid;
}

mach_port_t rx_mem_scan::target_task() {
    return _target_task;
}

boolean_t rx_mem_scan::is_idle() {
    return _idle;
}

search_result_t rx_mem_scan::first_fuzzy_search() {
    return search(rx_search_fuzzy_val, rx_compare_type_void);
}

search_result_t rx_mem_scan::fuzzy_search(rx_compare_type ct) {
    return search(rx_search_fuzzy_val, ct);
}

search_result_t rx_mem_scan::last_search_result() {
    return _last_search_result;
}

void rx_mem_scan::set_search_value_type(rx_search_value_type *type_p) {
    _search_value_type_p = type_p;
    if (!is_idle()) {
        reset();
    }
}

rx_memory_page_pt rx_mem_scan::page_of_memory(vm_address_t address, uint32_t page_size) {
    rx_memory_page_pt page = new rx_memory_page_t;
    page->addresses = new address_list_t;
    page->data_size = page_size * _search_value_type_p->size_of_value();
    page->data = new data_t[page->data_size];
    data_pt page_data_itor_p = page->data;

    for (uint32_t i = 0; i < _regions_p->size(); ++i) {
        region_t region = (*_regions_p)[i];
        if (!region.writable) {
            continue;
        }

        data_pt region_data_p = NULL;
        data_pt region_data_itor_p = NULL;

        if (rx_in_range(region.address, address, address + page_size * _search_value_type_p->size_of_value())) {
            address = region.address;
        }
        while (rx_in_range(address, region.address, region.address + region.size) && page_size > 0) {
            if (region_data_p == NULL) {
                region_data_p = new data_t[region.size];
                vm_size_t read_count;
                kern_return_t ret = read_region(region_data_p, region, &read_count);
                if (ret != KERN_SUCCESS) {
                    // TODO: Handle error
                }
                region_data_itor_p = region_data_p + (address - region.address);
            }

            page->addresses->push_back(address);
            memcpy(page_data_itor_p, region_data_itor_p, _search_value_type_p->size_of_value());

            address += _search_value_type_p->size_of_value();
            region_data_itor_p += _search_value_type_p->size_of_value();
            page_data_itor_p += _search_value_type_p->size_of_value();
            --page_size;
        }

        if (region_data_p) {
            delete[] region_data_p;
        }

        if (page_size == 0) {
            break;
        }
    }

    return page;
}

rx_memory_page_pt rx_mem_scan::page_of_matched(uint32_t page_no, uint32_t page_size) {
    rx_memory_page_pt page = new rx_memory_page_t;
    page->addresses = new address_list_t;
    page->data_size = page_size * _search_value_type_p->size_of_value();
    page->data = (data_pt) malloc(page->data_size);

    uint32_t page_idx_begin = page_no * page_size;
    uint32_t page_idx_end = page_idx_begin + page_size;

    uint32_t region_idx_begin = 0;
    data_pt page_data_itor_p = page->data;
    for (uint32_t i = 0; i < _regions_p->size(); ++i) {
        region_t region = (*_regions_p)[i];
        if (!region.writable) {
            continue;
        }

        uint32_t region_idx_end = region_idx_begin + region.matched_count;

        data_pt region_data_p = NULL;
        while (rx_in_range(page_idx_begin, region_idx_begin, region_idx_end)) {
            if (region_data_p == NULL) {
                region_data_p = new data_t[region.size];
                vm_size_t read_count;
                kern_return_t ret = read_region(region_data_p, region, &read_count);
                if (ret != KERN_SUCCESS) {
                    // TODO: Handle error
                }
            }

            uint32_t matched_idx_idx = page_idx_begin - region_idx_begin;
            matched_off_t matched_idx = offset_of_matched_offsets(*region.matched_offs, matched_idx_idx);

            vm_offset_t offset = matched_idx;
            vm_address_t address = region.address + offset;
            search_val_pt val_p = &(region_data_p[offset]);

            page->addresses->push_back(address);
            memcpy(page_data_itor_p, val_p, _search_value_type_p->size_of_value());

            ++ page_idx_begin;
            page_data_itor_p += _search_value_type_p->size_of_value();

            if (page_idx_begin == page_idx_end) {
                delete[] region_data_p;
                goto ret;
            }
        }

        if (region_data_p) {
            delete[] region_data_p;
        }

        region_idx_begin = region_idx_end;
    }

    ret:

    return page;
}

kern_return_t rx_mem_scan::write_val(vm_address_t address, search_val_pt val) {
    // TODO: Check address is writable.
    kern_return_t ret = vm_write(_target_task, address, (vm_offset_t) val, _search_value_type_p->size_of_value());
    return ret;
}

void rx_mem_scan::set_last_search_val(search_val_pt new_p) {
    if (!_last_search_val_p) {
        _last_search_val_p = malloc(_search_value_type_p->size_of_value());
    }
    memcpy(_last_search_val_p, new_p, _search_value_type_p->size_of_value());
}

search_result_t rx_mem_scan::search(search_val_pt search_val_p, rx_compare_type ct) {
    search_result_t result = { 0 };
    long begin_time = get_timestamp();
    bool is_fuzzy_search = rx_is_fuzzy_search_val(search_val_p);
    regions_pt used_regions = new regions_t();
    rx_comparator* comparator = _search_value_type_p->create_comparator(ct);

    std::cout << "开始搜索, 类型: " << (is_fuzzy_search ? "模糊搜索" : "精确搜索") << std::endl;
    if (!is_fuzzy_search) {
        std::cout << "搜索值: " << *(int*)search_val_p << std::endl;
    }

    if (is_fuzzy_search) {
        if (_idle) {
            _unknown_last_search_val = true;
        } else if (!_unknown_last_search_val) {
            search_val_p = _last_search_val_p;
        }
    } else {
        _unknown_last_search_val = false;
        set_last_search_val(search_val_p);
    }

    for (uint32_t i = 0; i < _regions_p->size(); ++i) {
        region_t region = (*_regions_p)[i];
        if (!region.writable) {
            continue;
        }

        std::cout << "搜索区域: 0x" << std::hex << region.address 
                  << " - 0x" << (region.address + region.size) 
                  << " 大小: " << std::dec << region.size << " 字节" << std::endl;

        size_t size_of_value = _search_value_type_p->size_of_value();
        size_t data_count = region.size / size_of_value;

        vm_size_t raw_data_read_count;
        data_pt region_data_p = new data_t[region.size];
        kern_return_t ret = read_region(region_data_p, region, &raw_data_read_count);

        if (ret == KERN_SUCCESS) {
            if (i == 0) {
                dump_memory(region.address, std::min(region.size, (vm_size_t)100));
            }

            matched_offs_pt matched_offs_p = new matched_offs_t;
            data_pt data_itor_p = region_data_p;
            uint32_t matched_count = 0;

            matched_offs_context_t matched_offs_context = { 0 };
            bzero(&matched_offs_context, sizeof(matched_offs_context_t));

            if (_idle) {
                if (is_fuzzy_search) {
                    matched_count = data_count;
                    add_matched_offs_multi(*matched_offs_p, 0, data_count);
                } else {
                    matched_off_t idx = 0;
                    data_pt end_p = (region_data_p + region.size);
                    while (data_itor_p < end_p) {
                        int current_value = *(int*)data_itor_p;
                        std::cout << "当前值: " << current_value << " 在地址: 0x" 
                                  << std::hex << (region.address + idx) << std::endl;
                        if (comparator->compare(search_val_p, data_itor_p)) {
                            std::cout << "找到匹配: 地址 0x" << std::hex << (region.address + idx) 
                                      << " 值: " << std::dec << current_value << std::endl;
                            ++matched_count;
                            add_matched_off(idx, matched_offs_context, *matched_offs_p);
                        }
                        data_itor_p += size_of_value;
                        idx += size_of_value;
                    }
                }
            } else {
                // Search again logic (unchanged)
            }

            free_region_memory(region);

            if (matched_count > 0) {
                if (_unknown_last_search_val) {
                    // Compression logic (unchanged)
                }

                matched_offs_flush(matched_offs_context, *matched_offs_p);
                region.matched_offs = matched_offs_p;
                region.matched_count = matched_count;

                result.memory_used += region.cal_memory_used();
                result.matched += matched_count;

                used_regions->push_back(region);
            }
        } else {
            std::cerr << "无法读取内存区域: 0x" << std::hex << region.address 
                      << ", 大小: " << std::dec << region.size << ", 错误: " << ret << std::endl;
        }

        delete[] region_data_p;
    }

    delete comparator;
    delete _regions_p;
    _regions_p = used_regions;
    _idle = false;

    long end_time = get_timestamp();
    result.time_used = end_time - begin_time;

    std::cout << "搜索完成, 总匹配数: " << result.matched << ", 耗时: " << result.time_used 
              << " 毫秒, 内存使用: " << result.memory_used << " 字节" << std::endl;

    _last_search_result = result;
    return result;
}
void rx_mem_scan::search_str(const std::string& str) {
    int matched_count = 0;
    long begin_time = get_timestamp();

    std::cout << "开始搜索字符串: \"" << str << "\"" << std::endl;

    for (uint32_t i = 0; i < _regions_p->size(); ++i) {
        region_t region = (*_regions_p)[i];

        std::cout << "搜索字符串区域: 0x" << std::hex << region.address << " - 0x" << (region.address + region.size) << std::dec << std::endl;

        vm_size_t raw_data_read_count;
        data_pt region_data_p = new data_t[region.size];
        kern_return_t ret = read_region(region_data_p, region, &raw_data_read_count);

        if (ret == KERN_SUCCESS) {
            // 添加内存dump
            if (i == 0) {  // 只dump第一个区域的前100字节作为示例
                dump_memory(region.address, std::min(region.size, (vm_size_t)100));
            }

            data_pt data_itor_p = region_data_p;
            int str_len = str.length();            

            while (raw_data_read_count >= str_len) {
                data_pt str_itor_p = (data_pt)str.c_str();
                bool found = true;

                int i = 0;
                while (i < str_len)
                {
                    if (::toupper(data_itor_p[i]) != str_itor_p[i])
                    {
                        found = false;
                        break;
                    }

                    ++i;
                }

                if (found)
                {
                    ++matched_count;

                    while (i < 255 + str_len && i < raw_data_read_count)
                    {
                        if (data_itor_p[i++] < 32) // fast skip invalid char, for next compare
                        {
                            break;
                        }
                    }

                    int j = -1;
                    while (j > -256 && &data_itor_p[j] >= region_data_p && data_itor_p[j] >= 32) {
                        --j;
                    }

                    std::cout << "找到匹配: 地址 0x" << std::hex << (data_itor_p - region_data_p + (uint64_t)region.address) << std::dec << ", 上下文: ";

                    char str_buff[256];                    
                    
                    memcpy(str_buff, &data_itor_p[j + 1], -j - 1);
                    str_buff[-j - 1] = 0;
                    std::cout << str_buff;

                    memcpy(str_buff, &data_itor_p[0], str_len);
                    str_buff[str_len] = 0;
                    std::cout << "\033[1;32m" << str_buff << "\033[0m";

                    memcpy(str_buff, &data_itor_p[str_len], i - str_len);
                    str_buff[i - str_len] = 0;
                    std::cout << str_buff << std::endl;

                    raw_data_read_count -= i;
                    data_itor_p += i;
                } else {
                    raw_data_read_count -= 1;
                    data_itor_p += 1;                    
                }
            }
        } else {
            std::cout << "读取失败: 地址 0x" << std::hex << region.address << ", 大小: " << region.size << std::dec << std::endl;
        }

        delete[] region_data_p;
    }

    long end_time = get_timestamp();
    std::cout << "字符串搜索完成, 匹配数: " << matched_count << ", 耗时: " << (float)(end_time - begin_time)/1000.0f << " 秒" << std::endl;
}

matched_off_t rx_mem_scan::offset_of_matched_offsets(matched_offs_t &vec, uint32_t off_idx) {
    uint32_t v_idx = 0;
    uint32_t b_off_idx = 0;
    while (v_idx < vec.size()) {
        matched_off_t b_off = vec[v_idx];
        if (rx_has_offc_mask(b_off)) {
            b_off = rx_remove_offc_mask(b_off);
            matched_off_ct off_c = vec[v_idx + 1];
            if (rx_in_range(off_idx, b_off_idx, b_off_idx + off_c)) {
                return b_off + (off_idx - b_off_idx) * _search_value_type_p->size_of_value();
            } else {
                b_off_idx += off_c;
                v_idx += 2;
            }
        } else {
            if (b_off_idx == off_idx) {
                return b_off;
            } else {
                b_off_idx += 1;
                v_idx += 1;
            }
        }
    }
    return 0;
}

void rx_mem_scan::free_regions() {
    if (_regions_p) {
        for (uint32_t i = 0; i < _regions_p->size(); ++i) {
            region_t region = (*_regions_p)[i];
            free_region_memory(region);
        }
        delete _regions_p;
        _regions_p = NULL;
    }
}

inline void rx_mem_scan::free_region_memory(region_t &region) {
    if (region.compressed_region_data) {
        delete[] region.compressed_region_data;
        region.compressed_region_data = NULL;
    }
    if (region.matched_offs) {
        delete region.matched_offs;
        region.matched_offs = NULL;
    }
}

inline void rx_mem_scan::matched_offs_flush(matched_offs_context_t &ctx, matched_offs_t &vec) {
    if (ctx.bf_nn) {
        if (ctx.fc > 0) {
            add_matched_offs_multi(vec, ctx.bf, ctx.fc + 1);
            ctx.fc = 0;
        } else {
            vec.push_back(ctx.bf);
        }
        ctx.bf_nn = false;
    }
}

inline void rx_mem_scan::add_matched_offs_multi(matched_offs_t &vec, matched_off_t bi, matched_off_ct ic) {
    vec.push_back(rx_add_offc_mask(bi));
    vec.push_back(ic);
}

inline void rx_mem_scan::add_matched_off(matched_off_t matched_idx, matched_offs_context_t &ctx, matched_offs_t &vec) {
    if (ctx.bf_nn) {
        if ((matched_idx - ctx.lf) == _search_value_type_p->size_of_value()) {
            ctx.fc ++;
        } else {
            matched_offs_flush(ctx, vec);
            ctx.bf = matched_idx;
        }
    } else {
        ctx.bf = matched_idx;
    }

    ctx.bf_nn = true;
    ctx.lf = matched_idx;
}

inline kern_return_t rx_mem_scan::read_region(data_pt region_data, region_t &region, vm_size_t *read_count) {
    kern_return_t ret = vm_read_overwrite(_target_task,
            region.address,
            region.size,
            (vm_address_t) region_data,
            read_count);
    return ret;
}

void rx_mem_scan::init_regions() {
    vm_address_t address = 0;
    vm_size_t size;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object_name;

    _regions_p = new regions_t();

    while (true) {
        kern_return_t kr = vm_region_64(
            _target_task,
            &address,
            &size,
            VM_REGION_BASIC_INFO_64,
            (vm_region_info_t)&info,
            &info_count,
            &object_name
        );

        if (kr != KERN_SUCCESS) {
            break;
        }

        if (info.protection & VM_PROT_READ) {
            region_t region;
            region.address = address;
            region.size = size;
            region.writable = info.protection & VM_PROT_WRITE;
            _regions_p->push_back(region);

            _trace("%016llx - %016llx, %d\n", address, address + size, region.writable);
        }

        address += size;
    }

    _trace("Readable region count: %d\n", (int)_regions_p->size());
}

// 在 rx_mem_scan 类中添加一个新的成员函数
void rx_mem_scan::dump_memory(vm_address_t address, size_t size) {
    data_pt buffer = new data_t[size];
    vm_size_t read_count;
    kern_return_t kr = vm_read_overwrite(_target_task, address, size, (vm_address_t)buffer, &read_count);
    
    if (kr != KERN_SUCCESS) {
        std::cerr << "内存读取失败，地址: 0x" << std::hex << address << std::dec << ", 错误码: " << kr << std::endl;
        delete[] buffer;
        return;
    }

    std::cout << "内存内容 (地址: 0x" << std::hex << address << std::dec << ", 大小: " << size << " 字节):" << std::endl;
    for (size_t i = 0; i < size; i += 16) {
        std::cout << std::hex << std::setw(8) << std::setfill('0') << (address + i) << ": ";
        for (size_t j = 0; j < 16 && (i + j) < size; ++j) {
            std::cout << std::setw(2) << std::setfill('0') << (int)buffer[i + j] << " ";
        }
        std::cout << "  ";
        for (size_t j = 0; j < 16 && (i + j) < size; ++j) {
            char c = buffer[i + j];
            std::cout << (isprint(c) ? c : '.');
        }
        std::cout << std::endl;
    }
    delete[] buffer;
}