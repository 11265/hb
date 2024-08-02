#include "rx_mem_scan.h"
#include "lz4/lz4.h"
#include <sys/time.h>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <fstream>
#include <chrono>

static long get_timestamp() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

rx_mem_scan::rx_mem_scan() 
    : _idle(true), _regions_p(nullptr), _search_value_type_p(nullptr), _last_search_val_p(nullptr),
      _stop_search(false), _pause_search(false), _search_progress(0.0f) {
}

rx_mem_scan::~rx_mem_scan() {
    stop_search_threads();
    free_memory();
}

void rx_mem_scan::free_memory() {
    free_regions();
    if (_last_search_val_p) {
        free(_last_search_val_p);
        _last_search_val_p = nullptr;
    }
    _memory_cache.clear();
}

void rx_mem_scan::reset() {
    free_memory();
    init_regions();
    _idle = true;
    _search_progress = 0.0f;
}

boolean_t rx_mem_scan::attach(pid_t pid) {
    _target_pid = pid;
    kern_return_t ret = task_for_pid(mach_task_self(), pid, &_target_task);
    if (ret != KERN_SUCCESS) {
        log("Attach to: " + std::to_string(pid) + " Failed: " + std::to_string(ret) + " " + mach_error_string(ret));
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

        data_pt region_data_p = get_cached_data(region.address, region.size);
        data_pt region_data_itor_p = region_data_p;

        if (rx_in_range(region.address, address, address + page_size * _search_value_type_p->size_of_value())) {
            address = region.address;
        }
        while (rx_in_range(address, region.address, region.address + region.size) && page_size > 0) {
            page->addresses->push_back(address);
            memcpy(page_data_itor_p, region_data_itor_p, _search_value_type_p->size_of_value());

            address += _search_value_type_p->size_of_value();
            region_data_itor_p += _search_value_type_p->size_of_value();
            page_data_itor_p += _search_value_type_p->size_of_value();
            --page_size;
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

        data_pt region_data_p = get_cached_data(region.address, region.size);
        while (rx_in_range(page_idx_begin, region_idx_begin, region_idx_end)) {
            uint32_t matched_idx_idx = page_idx_begin - region_idx_begin;
            matched_off_t matched_idx = offset_of_matched_offsets(*region.matched_offs, matched_idx_idx);

            vm_offset_t offset = matched_idx;
            vm_address_t address = region.address + offset;
            search_val_pt val_p = &(region_data_p[offset]);

            page->addresses->push_back(address);
            memcpy(page_data_itor_p, val_p, _search_value_type_p->size_of_value());

            ++page_idx_begin;
            page_data_itor_p += _search_value_type_p->size_of_value();

            if (page_idx_begin == page_idx_end) {
                goto ret;
            }
        }

        region_idx_begin = region_idx_end;
    }

ret:
    return page;
}

kern_return_t rx_mem_scan::write_val(vm_address_t address, search_val_pt val) {
    kern_return_t ret = vm_write(_target_task, address, (vm_offset_t) val, _search_value_type_p->size_of_value());
    if (ret == KERN_SUCCESS) {
        // 更新缓存
        auto it = _memory_cache.find(address);
        if (it != _memory_cache.end()) {
            memcpy(it->second.data(), val, _search_value_type_p->size_of_value());
        }
    }
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

        data_pt region_data_p = get_cached_data(region.address, region.size);

        if (region_data_p) {
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
                // 实现再次搜索的逻辑
            }

            if (matched_count > 0) {
                if (_unknown_last_search_val) {
                    // 实现压缩逻辑
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
                      << ", 大小: " << std::dec << region.size << std::endl;
        }
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

        data_pt region_data_p = get_cached_data(region.address, region.size);

        if (region_data_p) {
            if (i == 0) {
                dump_memory(region.address, std::min(region.size, (vm_size_t)100));
            }

            data_pt data_itor_p = region_data_p;
            int str_len = str.length();            
            vm_size_t remaining_size = region.size;

            while (remaining_size >= str_len) {
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

                    while (i < 255 + str_len && i < remaining_size && data_itor_p[i] >= 32) {
                        ++i;
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

                    remaining_size -= i;
                    data_itor_p += i;
                } else {
                    remaining_size -= 1;
                    data_itor_p += 1;                    
                }
            }
        } else {
            std::cout << "读取失败: 地址 0x" << std::hex << region.address << ", 大小: " << region.size << std::dec << std::endl;
        }
    }

    long end_time = get_timestamp();
    std::cout << "字符串搜索完成, 匹配数: " << matched_count << ", 耗时: " << (float)(end_time - begin_time)/1000.0f << " 秒" << std::endl;
}

void rx_mem_scan::dump_memory(vm_address_t address, size_t size) {
    data_pt buffer = get_cached_data(address, size);
    if (!buffer) {
        std::cerr << "内存读取失败，地址: 0x" << std::hex << address << std::dec << std::endl;
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
}

search_result_t rx_mem_scan::range_search(int min, int max) {
    SearchTask task;
    task.type = SearchTask::Type::Range;
    task.range = {min, max};
    _search_tasks.push(task);
    start_search_threads();
    return wait_for_search_result();
}

search_result_t rx_mem_scan::fuzzy_search(int value, int tolerance) {
    SearchTask task;
    task.type = SearchTask::Type::Fuzzy;
    task.fuzzy = {value, tolerance};
    _search_tasks.push(task);
    start_search_threads();
    return wait_for_search_result();
}

search_result_t rx_mem_scan::multi_value_search(const std::vector<int>& values) {
    SearchTask task;
    task.type = SearchTask::Type::MultiValue;
    task.multi_value = new MultiValueSearch{values};
    _search_tasks.push(task);
    start_search_threads();
    search_result_t result = wait_for_search_result();
    delete task.multi_value;
    return result;
}

search_result_t rx_mem_scan::incremental_search(search_val_pt search_val_p, rx_compare_type ct) {
    SearchTask task;
    task.type = SearchTask::Type::Exact;
    task.exact_value = *(int*)search_val_p;
    _search_tasks.push(task);
    start_search_threads();
    return wait_for_search_result();
}

void rx_mem_scan::set_search_regions(const std::vector<std::pair<vm_address_t, vm_size_t>>& regions) {
    std::lock_guard<std::mutex> lock(_search_mutex);
    delete _regions_p;
    _regions_p = new regions_t();
    for (const auto& region : regions) {
        rx_region new_region;
        new_region.address = region.first;
        new_region.size = region.second;
        new_region.writable = true;  // 假设所有指定的区域都是可写的
        _regions_p->push_back(new_region);
    }
}

void rx_mem_scan::set_result_callback(std::function<void(vm_address_t, int)> callback) {
    _result_callback = callback;
}

void rx_mem_scan::pause_search() {
    _pause_search = true;
}

void rx_mem_scan::resume_search() {
    _pause_search = false;
    _search_cv.notify_all();
}

float rx_mem_scan::get_search_progress() const {
    return _search_progress.load();
}

std::vector<uint8_t> rx_mem_scan::view_memory(vm_address_t address, size_t size) {
    data_pt buffer = get_cached_data(address, size);
    if (!buffer) {
        log("Failed to read memory at address: " + std::to_string(address));
        return {};
    }
    return std::vector<uint8_t>(buffer, buffer + size);
}

bool rx_mem_scan::edit_memory(vm_address_t address, const std::vector<uint8_t>& data) {
    kern_return_t kr = vm_write(_target_task, address, (vm_offset_t)data.data(), data.size());
    if (kr != KERN_SUCCESS) {
        log("Failed to write memory at address: " + std::to_string(address));
        return false;
    }
    
    // 更新缓存
    auto it = _memory_cache.find(address);
    if (it != _memory_cache.end()) {
        std::copy(data.begin(), data.end(), it->second.begin());
    }
    
    return true;
}

void rx_mem_scan::thread_search_task() {
    while (!_stop_search) {
        SearchTask task;
        {
            std::unique_lock<std::mutex> lock(_search_mutex);
            _search_cv.wait(lock, [this] { return !_search_tasks.empty() || _stop_search; });
            if (_stop_search) break;
            task = _search_tasks.front();
            _search_tasks.pop();
        }

        search_result_t result = {0};
        switch (task.type) {
            case SearchTask::Type::Exact:
                result = search(&task.exact_value, rx_compare_type_eq);
                break;
            case SearchTask::Type::Range:
                // 实现范围搜索
                break;
            case SearchTask::Type::Fuzzy:
                // 实现模糊搜索
                break;
            case SearchTask::Type::MultiValue:
                // 实现多值搜索
                break;
        }

        if (_result_callback) {
            for (const auto& region : *_regions_p) {
                for (uint32_t i = 0; i < region.matched_count; ++i) {
                    vm_address_t address = region.address + offset_of_matched_offsets(*region.matched_offs, i);
                    int value = *(int*)get_cached_data(address, sizeof(int));
                    _result_callback(address, value);
                }
            }
        }

        _search_progress = 1.0f;
        _search_cv.notify_all();
    }
}

void rx_mem_scan::start_search_threads() {
    _stop_search = false;
    _search_progress = 0.0f;
    int thread_count = std::thread::hardware_concurrency();
    for (int i = 0; i < thread_count; ++i) {
        _search_threads.emplace_back(&rx_mem_scan::thread_search_task, this);
    }
}

void rx_mem_scan::stop_search_threads() {
    _stop_search = true;
    _search_cv.notify_all();
    for (auto& thread : _search_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    _search_threads.clear();
}

search_result_t rx_mem_scan::wait_for_search_result() {
    std::unique_lock<std::mutex> lock(_search_mutex);
    _search_cv.wait(lock, [this] { return _search_progress >= 1.0f; });
    return _last_search_result;
}

void rx_mem_scan::cache_memory_region(const region_t& region) {
    std::vector<uint8_t> data(region.size);
    vm_size_t read_size;
    kern_return_t kr = vm_read_overwrite(_target_task, region.address, region.size, (vm_address_t)data.data(), &read_size);
    if (kr == KERN_SUCCESS && read_size == region.size) {
        _memory_cache[region.address] = std::move(data);
    } else {
        log("Failed to cache memory region: " + std::to_string(region.address));
    }
}

data_pt rx_mem_scan::get_cached_data(vm_address_t address, size_t size) {
    auto it = _memory_cache.lower_bound(address);
    if (it != _memory_cache.begin()) {
        --it;
    }
    if (it != _memory_cache.end() && address >= it->first && address + size <= it->first + it->second.size()) {
        return it->second.data() + (address - it->first);
    }
    
    // 如果缓存中没有，则读取并缓存
    region_t region;
    region.address = address;
    region.size = size;
    cache_memory_region(region);
    
    it = _memory_cache.find(address);
    if (it != _memory_cache.end()) {
        return it->second.data();
    }
    
    return nullptr;
}

void rx_mem_scan::compress_results() {
    for (auto& region : *_regions_p) {
        if (region.matched_offs && region.matched_offs->size() > 1000) {
            // 使用 LZ4 压缩
            int max_dst_size = LZ4_compressBound(region.matched_offs->size() * sizeof(matched_off_t));
            char* compressed_data = new char[max_dst_size];
            int compressed_size = LZ4_compress_default(
                (const char*)region.matched_offs->data(),
                compressed_data,
                region.matched_offs->size() * sizeof(matched_off_t),
                max_dst_size
            );
            if (compressed_size > 0) {
                delete[] region.compressed_region_data;
                region.compressed_region_data = (raw_data_pt)compressed_data;
                region.compressed_region_data_size = compressed_size;
                delete region.matched_offs;
                region.matched_offs = nullptr;
            } else {
                delete[] compressed_data;
            }
        }
    }
}

void rx_mem_scan::decompress_results() {
    for (auto& region : *_regions_p) {
        if (region.compressed_region_data && !region.matched_offs) {
            int original_size = region.matched_count * sizeof(matched_off_t);
            matched_offs_pt decompressed_offs = new matched_offs_t(region.matched_count);
            int decompressed_size = LZ4_decompress_safe(
                (const char*)region.compressed_region_data,
                (char*)decompressed_offs->data(),
                region.compressed_region_data_size,
                original_size
            );
            if (decompressed_size == original_size) {
                delete[] region.compressed_region_data;
                region.compressed_region_data = nullptr;
                region.compressed_region_data_size = 0;
                region.matched_offs = decompressed_offs;
            } else {
                delete decompressed_offs;
            }
        }
    }
}

void rx_mem_scan::detect_patterns() {
    // 简单的模式识别示例：检测等差数列
    for (const auto& region : *_regions_p) {
        if (region.matched_count < 3) continue;

        std::vector<int> values;
        for (uint32_t i = 0; i < region.matched_count; ++i) {
            vm_address_t address = region.address + offset_of_matched_offsets(*region.matched_offs, i);
            int value = *(int*)get_cached_data(address, sizeof(int));
            values.push_back(value);
        }

        int diff = values[1] - values[0];
        bool is_arithmetic = true;
        for (size_t i = 2; i < values.size(); ++i) {
            if (values[i] - values[i-1] != diff) {
                is_arithmetic = false;
                break;
            }
        }

        if (is_arithmetic) {
            log("检测到等差数列模式：起始值 = " + std::to_string(values[0]) + ", 公差 = " + std::to_string(diff));
        }
    }
}

void rx_mem_scan::handle_memory_access_error(vm_address_t address) {
    log("内存访问错误：地址 0x" + std::to_string(address));
    // 可以在这里实现重试逻辑或其他错误处理机制
}

void rx_mem_scan::log(const std::string& message) {
    std::cout << "[" << get_timestamp() << "] " << message << std::endl;
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
        _regions_p = nullptr;
    }
}

void rx_mem_scan::free_region_memory(region_t &region) {
    if (region.compressed_region_data) {
        delete[] region.compressed_region_data;
        region.compressed_region_data = nullptr;
    }
    if (region.matched_offs) {
        delete region.matched_offs;
        region.matched_offs = nullptr;
    }
}

void rx_mem_scan::matched_offs_flush(matched_offs_context_t &ctx, matched_offs_t &vec) {
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

void rx_mem_scan::add_matched_offs_multi(matched_offs_t &vec, matched_off_t bi, matched_off_ct ic) {
    vec.push_back(rx_add_offc_mask(bi));
    vec.push_back(ic);
}

void rx_mem_scan::add_matched_off(matched_off_t matched_idx, matched_offs_context_t &ctx, matched_offs_t &vec) {
    if (ctx.bf_nn) {
        if ((matched_idx - ctx.lf) == _search_value_type_p->size_of_value()) {
            ctx.fc++;
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

kern_return_t rx_mem_scan::read_region(data_pt region_data, region_t &region, vm_size_t *read_count) {
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

            log("Region: " + std::to_string(address) + " - " + std::to_string(address + size) + ", Writable: " + std::to_string(region.writable));
        }

        address += size;
    }

    log("Readable region count: " + std::to_string(_regions_p->size()));
}

// 新增：智能过滤
void rx_mem_scan::smart_filter_regions() {
    regions_t filtered_regions;
    for (const auto& region : *_regions_p) {
        // 示例：过滤掉小于 1KB 的区域
        if (region.size >= 1024) {
            filtered_regions.push_back(region);
        }
    }
    delete _regions_p;
    _regions_p = new regions_t(std::move(filtered_regions));
}

// 新增：二分查找优化
template<typename T>
bool rx_mem_scan::binary_search_region(const region_t& region, T value) {
    data_pt data = get_cached_data(region.address, region.size);
    if (!data) return false;

    T* start = reinterpret_cast<T*>(data);
    T* end = start + (region.size / sizeof(T));

    return std::binary_search(start, end, value);
}

// 新增：性能分析辅助函数
void rx_mem_scan::performance_analysis() {
    auto start = std::chrono::high_resolution_clock::now();
    
    // 执行搜索操作
    search(nullptr, rx_compare_type_eq);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    log("搜索操作耗时: " + std::to_string(duration.count()) + " 毫秒");
}

// 新增：异常处理包装函数
template<typename Func>
void rx_mem_scan::safe_execute(Func func) {
    try {
        func();
    } catch (const std::exception& e) {
        log("异常: " + std::string(e.what()));
    } catch (...) {
        log("未知异常发生");
    }
}

// 主搜索函数的异常安全版本
search_result_t rx_mem_scan::safe_search(search_val_pt search_val_p, rx_compare_type ct) {
    search_result_t result = {0};
    safe_execute([&]() {
        result = search(search_val_p, ct);
    });
    return result;
}

// 新增：自动重试机制
template<typename Func>
auto rx_mem_scan::retry_operation(Func func, int max_retries = 3) -> decltype(func()) {
    for (int i = 0; i < max_retries; ++i) {
        try {
            return func();
        } catch (const std::exception& e) {
            log("操作失败，尝试重试 (" + std::to_string(i+1) + "/" + std::to_string(max_retries) + "): " + e.what());
            if (i == max_retries - 1) throw;
        }
    }
    throw std::runtime_error("达到最大重试次数");
}

// 使用示例：
// auto result = retry_operation([this]() { return this->search(search_val_p, ct); });

// 新增：内存使用优化
void rx_mem_scan::optimize_memory_usage() {
    // 压缩不常用的搜索结果
    compress_results();

    // 清理不需要的缓存
    for (auto it = _memory_cache.begin(); it != _memory_cache.end();) {
        if (/* 某些条件，例如长时间未使用 */) {
            it = _memory_cache.erase(it);
        } else {
            ++it;
        }
    }
}

// 新增：搜索策略优化
search_result_t rx_mem_scan::optimized_search(search_val_pt search_val_p, rx_compare_type ct) {
    // 首先尝试快速搜索（例如，只搜索部分内存）
    auto quick_result = quick_search(search_val_p, ct);
    if (quick_result.matched > 0) {
        return quick_result;
    }

    // 如果快速搜索失败，则进行完整搜索
    return search(search_val_p, ct);
}

search_result_t rx_mem_scan::quick_search(search_val_pt search_val_p, rx_compare_type ct) {
    // 实现快速搜索逻辑，例如只搜索最可能包含目标值的区域
    // ...
}

// 新增：搜索结果可视化
void rx_mem_scan::visualize_results() {
    // 这里只是一个简单的示例，实际实现可能需要使用图形库
    for (const auto& region : *_regions_p) {
        std::cout << "Region: " << std::hex << region.address << " - " << (region.address + region.size) << std::dec << std::endl;
        std::cout << "Matches: ";
        for (uint32_t i = 0; i < std::min(region.matched_count, 10u); ++i) {
            std::cout << std::hex << (region.address + offset_of_matched_offsets(*region.matched_offs, i)) << " ";
        }
        if (region.matched_count > 10) std::cout << "...";
        std::cout << std::dec << std::endl;
    }
}

// 新增：搜索历史记录
void rx_mem_scan::add_to_search_history(search_val_pt search_val_p, rx_compare_type ct, const search_result_t& result) {
    _search_history.push_back({search_val_p, ct, result, std::chrono::system_clock::now()});
    if (_search_history.size() > 100) {  // 保留最近100次搜索
        _search_history.pop_front();
    }
}

// 新增：获取搜索历史
std::vector<search_history_entry> rx_mem_scan::get_search_history() const {
    return std::vector<search_history_entry>(_search_history.begin(), _search_history.end());
}

// 新增：保存和加载搜索状态
void rx_mem_scan::save_state(const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("无法创建文件: " + filename);
    }

    // 保存基本信息
    file.write(reinterpret_cast<const char*>(&_target_pid), sizeof(_target_pid));
    // 保存区域信息
    size_t region_count = _regions_p->size();
    file.write(reinterpret_cast<const char*>(&region_count), sizeof(region_count));
    for (const auto& region : *_regions_p) {
        file.write(reinterpret_cast<const char*>(&region), sizeof(region));
        // 保存匹配偏移
        if (region.matched_offs) {
            size_t offs_size = region.matched_offs->size();
            file.write(reinterpret_cast<const char*>(&offs_size), sizeof(offs_size));
            file.write(reinterpret_cast<const char*>(region.matched_offs->data()), offs_size * sizeof(matched_off_t));
        }
    }
    // 可以添加更多需要保存的状态信息
}

void rx_mem_scan::load_state(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("无法打开文件: " + filename);
    }

    // 加载基本信息
    file.read(reinterpret_cast<char*>(&_target_pid), sizeof(_target_pid));
    // 重新附加到进程
    if (!attach(_target_pid)) {
        throw std::runtime_error("无法重新附加到进程");
    }
    // 加载区域信息
    size_t region_count;
    file.read(reinterpret_cast<char*>(&region_count), sizeof(region_count));
    delete _regions_p;
    _regions_p = new regions_t();
    for (size_t i = 0; i < region_count; ++i) {
        region_t region;
        file.read(reinterpret_cast<char*>(&region), sizeof(region));
        // 加载匹配偏移
        size_t offs_size;
        file.read(reinterpret_cast<char*>(&offs_size), sizeof(offs_size));
        region.matched_offs = new matched_offs_t(offs_size);
        file.read(reinterpret_cast<char*>(region.matched_offs->data()), offs_size * sizeof(matched_off_t));
        _regions_p->push_back(region);
    }
    // 可以添加更多需要加载的状态信息
}

// 结束 rx_mem_scan.cpp 文件