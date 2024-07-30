#ifndef 内存模块_H
#define 内存模块_H

#include <stdint.h>
#include <sys/types.h>
#include <mach/mach.h>

typedef struct {
    vm_address_t 基地址;
    void* 映射内存;
    size_t 映射大小;
    uint32_t 访问次数;
    time_t 最后访问时间;
} 内存区域;

typedef struct {
    int 操作;  // 0 表示读取，1 表示写入
    vm_address_t 地址;
    void* 缓冲区;
    size_t 大小;
    void* 结果;
} 内存请求;

typedef struct {
    size_t 页面大小;
    int 线程数量;
    int 最大等待请求数;
    int 初始缓存区域数;
} 内存模块配置;

// 初始化内存模块
int 初始化内存模块(pid_t pid, 内存模块配置* 配置);

// 关闭内存模块
void 关闭内存模块();

// 读取指定地址的内存
void* 读任意地址(vm_address_t 地址, size_t 大小);

// 写入指定地址的内存
int 写任意地址(vm_address_t 地址, const void* 数据, size_t 大小);

// 读取特定类型的内存
int32_t 读内存i32(vm_address_t 地址);
int64_t 读内存i64(vm_address_t 地址);
float   读内存f32(vm_address_t 地址);
double  读内存f64(vm_address_t 地址);

// 写入特定类型的内存
int 写内存i32(vm_address_t 地址, int32_t 值);
int 写内存i64(vm_address_t 地址, int64_t 值);
int 写内存f32(vm_address_t 地址, float 值);
int 写内存f64(vm_address_t 地址, double 值);

// 在指定范围内搜索内存
vm_address_t 查找内存(const void* 数据, size_t 大小, vm_address_t 开始地址, vm_address_t 结束地址);

// 读取进程内存
int 读取进程内存(vm_address_t 地址, void* 缓冲区, size_t 大小);

// 写入进程内存
int 写入进程内存(vm_address_t 地址, const void* 数据, size_t 大小);

// 清理未使用的页面
void 清理未使用页面(time_t 早于);

// 内存对齐函数
size_t 对齐(size_t 大小, size_t 对齐值);

#endif // 内存模块_H