#ifndef RXMEMSCAN_RX_MEM_SCAN_MINI_H
#define RXMEMSCAN_RX_MEM_SCAN_MINI_H

#include <mach/mach.h>
#include <memory>
#include <string>
#include <vector>
#include <mach/vm_region.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <functional>
#include <map>

#ifdef RXDEBUG
#   define _trace(s,...) (printf(s, __VA_ARGS__))
#else
#   define _trace(s,...)
#endif

typedef struct {
    boolean_t bf_nn;    // Base offset not null.
    uint32_t lf;        // Last offset.
    uint32_t bf;        // Base offset.
    uint32_t fc;        // Offset continuous.
} matched_offs_context_t;

typedef struct {
    uint32_t time_used;
    uint32_t memory_used;
    uint32_t matched;
} search_result_t;

class rx_region;

typedef uint8_t                     raw_data_t;
typedef raw_data_t *                raw_data_pt;

typedef uint32_t                    matched_off_t;
typedef uint32_t                    matched_off_ct;
typedef std::vector<matched_off_t>  matched_offs_t;
typedef matched_offs_t *            matched_offs_pt;

typedef rx_region                   region_t;
typedef std::vector<region_t>       regions_t;
typedef regions_t *                 regions_pt;

typedef std::vector<vm_address_t>   address_list_t;
typedef address_list_t * address_list_pt;
typedef void *                      search_val_pt;
typedef uint8_t                     data_t;
typedef uint8_t *                   data_pt;

class rx_memory_page {
public:
    rx_memory_page() : addresses(nullptr), data(nullptr), data_size(0) {}
    ~rx_memory_page() {
        delete addresses;
        delete[] data;
    }

public:
    address_list_pt addresses;
    data_pt         data;
    size_t          data_size;
};
typedef rx_memory_page              rx_memory_page_t;
typedef rx_memory_page_t *          rx_memory_page_pt;

class rx_region {
public:
    rx_region() : compressed_region_data(nullptr), matched_offs(nullptr), 
                  compressed_region_data_size(0), matched_count(0) {}

    vm_address_t address;
    vm_size_t size;
    boolean_t writable;

    raw_data_pt compressed_region_data;
    size_t compressed_region_data_size;

    matched_offs_pt matched_offs;
    uint32_t matched_count;

    size_t cal_memory_used() {
        return sizeof(vm_address_t) + sizeof(vm_address_t) + sizeof(uint32_t) + compressed_region_data_size
                + (matched_offs ?  (sizeof(matched_off_t) * matched_offs->size()) : 0);
    }
};

typedef enum {
    rx_compare_type_void,
    rx_compare_type_eq = 1,
    rx_compare_type_ne,
    rx_compare_type_lt,
    rx_compare_type_gt
} rx_compare_type;

class rx_comparator {
public:
    virtual ~rx_comparator() {}
    virtual boolean_t compare(void *a, void *b) = 0;
};

template <typename T>
class rx_comparator_typed_eq : public rx_comparator {
public:
    boolean_t compare(void *a, void *b) override {
        T value_a = *(T *)a;
        T value_b = *(T *)b;
        std::cout << "比较: " << value_a << " 和 " << value_b << std::endl;
        return value_a == value_b;
    }
};

template <typename T>
class rx_comparator_typed_ne : public rx_comparator {
public:
    boolean_t compare(void *a, void *b) override { return *(T *)b != *(T *)a; }
};

template <typename T>
class rx_comparator_typed_lt : public rx_comparator {
public:
    boolean_t compare(void *a, void *b) override { return *(T *)b < *(T *)a; }
};

template <typename T>
class rx_comparator_typed_gt : public rx_comparator {
public:
    boolean_t compare(void *a, void *b) override { return *(T *)b > *(T *)a; }
};

class rx_search_value_type {
public:
    virtual ~rx_search_value_type() {}
    virtual size_t size_of_value() = 0;
    virtual rx_comparator *create_comparator(rx_compare_type ct) = 0;
};

template <typename T>
class rx_search_typed_value_type : public rx_search_value_type {
public:
    size_t size_of_value() override { return sizeof(T); }
    rx_comparator *create_comparator(rx_compare_type ct) override {
        switch (ct) {
            case rx_compare_type_eq: return new rx_comparator_typed_eq<T>();
            case rx_compare_type_ne: return new rx_comparator_typed_ne<T>();
            case rx_compare_type_lt: return new rx_comparator_typed_lt<T>();
            case rx_compare_type_gt: return new rx_comparator_typed_gt<T>();
            default: return nullptr;
        }
    }
};

#ifdef __LP64__
#   define rx_offc_mask             0x8000000000000000
#else
#   define rx_offc_mask             0x80000000
#endif
#define rx_add_offc_mask(i)         ((i) | rx_offc_mask)
#define rx_remove_offc_mask(i)      ((i) & ~rx_offc_mask)
#define rx_has_offc_mask(i)         (((i) & rx_offc_mask) == rx_offc_mask)

#define rx_search_fuzzy_val         NULL
#define rx_is_fuzzy_search_val(_v)  ((_v)==rx_search_fuzzy_val)

#define rx_in_range(v, b, e)        ((v)>=(b) && (v)<(e))

struct SearchRange {
    int min;
    int max;
};

struct FuzzySearch {
    int value;
    int tolerance;
};

struct MultiValueSearch {
    std::vector<int> values;
};

struct SearchTask {
    enum class Type { Exact, Range, Fuzzy, MultiValue };
    Type type;
    union {
        int exact_value;
        SearchRange range;
        FuzzySearch fuzzy;
        MultiValueSearch* multi_value;
    };
};

class rx_mem_scan {
public:
    rx_mem_scan();
    ~rx_mem_scan();
    void free_memory();
    void reset();
    boolean_t attach(pid_t pid);
    boolean_t is_idle();
    search_result_t first_fuzzy_search();
    search_result_t fuzzy_search(rx_compare_type ct);
    search_result_t last_search_result();
    void set_search_value_type(rx_search_value_type *type_p);
    rx_memory_page_pt page_of_memory(vm_address_t address, uint32_t page_size);
    rx_memory_page_pt page_of_matched(uint32_t page_no, uint32_t page_size);
    kern_return_t write_val(vm_address_t address, search_val_pt val);
    void set_last_search_val(search_val_pt new_p);
    search_result_t search(search_val_pt search_val_p, rx_compare_type ct);
    void search_str(const std::string &str);
    void dump_memory(vm_address_t address, size_t size);
    pid_t target_pid();
    mach_port_t target_task();

    search_result_t range_search(int min, int max);
    search_result_t fuzzy_search(int value, int tolerance);
    search_result_t multi_value_search(const std::vector<int>& values);
    search_result_t incremental_search(search_val_pt search_val_p, rx_compare_type ct);
    void set_search_regions(const std::vector<std::pair<vm_address_t, vm_size_t>>& regions);
    void set_result_callback(std::function<void(vm_address_t, int)> callback);
    void pause_search();
    void resume_search();
    float get_search_progress() const;
    std::vector<uint8_t> view_memory(vm_address_t address, size_t size);
    bool edit_memory(vm_address_t address, const std::vector<uint8_t>& data);

private:
    matched_off_t offset_of_matched_offsets(matched_offs_t &vec, uint32_t off_idx);
    void free_region_memory(region_t &region);
    void matched_offs_flush(matched_offs_context_t &ctx, matched_offs_t &vec);
    void add_matched_offs_multi(matched_offs_t &vec, matched_off_t bi, matched_off_ct ic);
    void add_matched_off(matched_off_t matched_idx, matched_offs_context_t &ctx, matched_offs_t &vec);
    kern_return_t read_region(data_pt region_data, region_t &region, vm_size_t *read_count);
    void init_regions();
    void free_regions();

    void thread_search_task();
    void start_search_threads();
    void stop_search_threads();
    void cache_memory_region(const region_t& region);
    data_pt get_cached_data(vm_address_t address, size_t size);
    void compress_results();
    void decompress_results();
    void detect_patterns();
    void handle_memory_access_error(vm_address_t address);
    void log(const std::string& message);

    search_result_t wait_for_search_result();
    void smart_filter_regions();
    template<typename T>
    bool binary_search_region(const region_t& region, T value);
    void performance_analysis();
    template<typename Func>
    void safe_execute(Func func);
    search_result_t safe_search(search_val_pt search_val_p, rx_compare_type ct);
    template<typename Func>
    auto retry_operation(Func func, int max_retries = 3) -> decltype(func());
    void optimize_memory_usage();
    search_result_t optimized_search(search_val_pt search_val_p, rx_compare_type ct);
    search_result_t quick_search(search_val_pt search_val_p, rx_compare_type ct);
    void visualize_results();

private:
    pid_t                       _target_pid;
    mach_port_t                 _target_task;
    regions_pt                  _regions_p;
    search_result_t             _last_search_result;
    search_val_pt               _last_search_val_p;
    boolean_t                   _unknown_last_search_val;
    boolean_t                   _idle;
    rx_search_value_type *      _search_value_type_p;

    std::vector<std::thread> _search_threads;
    std::mutex _search_mutex;
    std::condition_variable _search_cv;
    std::atomic<bool> _stop_search;
    std::atomic<bool> _pause_search;
    std::queue<SearchTask> _search_tasks;
    std::function<void(vm_address_t, int)> _result_callback;
    std::atomic<float> _search_progress;
    std::map<vm_address_t, std::vector<uint8_t>> _memory_cache;
};

typedef rx_search_typed_value_type<int> rx_search_int_type;
typedef rx_search_typed_value_type<float> rx_search_float_type;
typedef rx_search_typed_value_type<double> rx_search_double_type;

#endif //RXMEMSCAN_RX_MEM_SCAN_MINI_H