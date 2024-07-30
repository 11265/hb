#include "内存模块.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <pthread.h>

// 默认配置
size_t 页面大小 = 4096;
int 线程数量 = 4;
int 最大等待请求数 = 1000;
int 初始缓存区域数 = 100;

typedef struct {
    pthread_t 线程;
    int id;
} 线程信息;

static 内存区域 *缓存区域 = NULL;
static int 缓存区域数量 = 0;
static int 最大缓存区域数 = 0;
static pid_t 目标进程ID;
static task_t 目标任务;

static 线程信息 *线程池 = NULL;
static pthread_mutex_t 区域互斥锁 = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t 请求互斥锁 = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t 请求条件变量 = PTHREAD_COND_INITIALIZER;

static 内存请求 *等待请求 = NULL;
static int 等待请求数量 = 0;
static int 停止线程 = 0;

// 内存池
#define 内存池大小 1048576  // 1MB
static char *内存池 = NULL;
static size_t 内存池已用 = 0;
static pthread_mutex_t 内存池互斥锁 = PTHREAD_MUTEX_INITIALIZER;

void* 内存池分配(size_t 大小) {
    pthread_mutex_lock(&内存池互斥锁);
    大小 = 对齐(大小, 8);  // 8字节对齐
    if (内存池已用 + 大小 > 内存池大小) {
        pthread_mutex_unlock(&内存池互斥锁);
        return malloc(大小);  // 如果内存池不足，回退到使用malloc
    }
    void *指针 = 内存池 + 内存池已用;
    内存池已用 += 大小;
    pthread_mutex_unlock(&内存池互斥锁);
    return 指针;
}

void 内存池重置() {
    pthread_mutex_lock(&内存池互斥锁);
    内存池已用 = 0;
    pthread_mutex_unlock(&内存池互斥锁);
}

内存区域* 获取或创建页面(vm_address_t 地址) {
    vm_address_t 页面地址 = 地址 & ~(页面大小 - 1);
    
    pthread_mutex_lock(&区域互斥锁);
    
    // 查找现有的页面
    for (int i = 0; i < 缓存区域数量; i++) {
        if (缓存区域[i].基地址 == 页面地址) {
            缓存区域[i].访问次数++;
            缓存区域[i].最后访问时间 = time(NULL);
            pthread_mutex_unlock(&区域互斥锁);
            return &缓存区域[i];
        }
    }
    
    // 如果没有找到，创建新的页面
    if (缓存区域数量 >= 最大缓存区域数) {
        // 如果缓存已满，移除最少使用的页面
        int 最少使用索引 = 0;
        uint32_t 最少使用次数 = UINT32_MAX;
        time_t 最早访问时间 = time(NULL);
        
        for (int i = 0; i < 缓存区域数量; i++) {
            if (缓存区域[i].访问次数 < 最少使用次数 ||
                (缓存区域[i].访问次数 == 最少使用次数 && 
                 缓存区域[i].最后访问时间 < 最早访问时间)) {
                最少使用索引 = i;
                最少使用次数 = 缓存区域[i].访问次数;
                最早访问时间 = 缓存区域[i].最后访问时间;
            }
        }
        
        munmap(缓存区域[最少使用索引].映射内存, 缓存区域[最少使用索引].映射大小);
        memmove(&缓存区域[最少使用索引], &缓存区域[最少使用索引 + 1], 
                (缓存区域数量 - 最少使用索引 - 1) * sizeof(内存区域));
        缓存区域数量--;
    }
    
    // 映射新页面
    void* 映射内存 = mmap(NULL, 页面大小, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (映射内存 == MAP_FAILED) {
        pthread_mutex_unlock(&区域互斥锁);
        return NULL;
    }
    
    mach_vm_size_t 读取字节数;
    kern_return_t kr = vm_read_overwrite(目标任务, 页面地址, 页面大小, (vm_address_t)映射内存, &读取字节数);
    if (kr != KERN_SUCCESS || 读取字节数 != 页面大小) {
        munmap(映射内存, 页面大小);
        pthread_mutex_unlock(&区域互斥锁);
        return NULL;
    }
    
    缓存区域[缓存区域数量].基地址 = 页面地址;
    缓存区域[缓存区域数量].映射内存 = 映射内存;
    缓存区域[缓存区域数量].映射大小 = 页面大小;
    缓存区域[缓存区域数量].访问次数 = 1;
    缓存区域[缓存区域数量].最后访问时间 = time(NULL);
    
    缓存区域数量++;
    
    pthread_mutex_unlock(&区域互斥锁);
    return &缓存区域[缓存区域数量 - 1];
}

void* 读任意地址(vm_address_t 地址, size_t 大小) {
    内存区域* 区域 = 获取或创建页面(地址);
    if (!区域) {
        return NULL;
    }
    
    size_t 偏移 = 地址 - 区域->基地址;
    void* 缓冲区 = 内存池分配(大小);
    if (!缓冲区) {
        return NULL;
    }
    
    if (偏移 + 大小 > 页面大小) {
        // 如果读取跨越了页面边界，我们需要分段读取
        size_t 第一部分 = 页面大小 - 偏移;
        memcpy(缓冲区, (char*)区域->映射内存 + 偏移, 第一部分);
        
        // 读取剩余部分
        size_t 剩余 = 大小 - 第一部分;
        void* 剩余数据 = 读任意地址(地址 + 第一部分, 剩余);
        if (!剩余数据) {
            return NULL;
        }
        
        memcpy((char*)缓冲区 + 第一部分, 剩余数据, 剩余);
    } else {
        // 如果读取没有跨越页面边界，直接复制数据
        memcpy(缓冲区, (char*)区域->映射内存 + 偏移, 大小);
    }
    
    return 缓冲区;
}

int 写任意地址(vm_address_t 地址, const void* 数据, size_t 大小) {
    内存区域* 区域 = 获取或创建页面(地址);
    if (!区域) {
        return -1;
    }
    
    size_t 偏移 = 地址 - 区域->基地址;
    
    if (偏移 + 大小 > 页面大小) {
        // 如果写入跨越了页面边界，我们需要分段写入
        size_t 第一部分 = 页面大小 - 偏移;
        memcpy((char*)区域->映射内存 + 偏移, 数据, 第一部分);
        
        // 写入剩余部分
        size_t 剩余 = 大小 - 第一部分;
        return 写任意地址(地址 + 第一部分, (char*)数据 + 第一部分, 剩余);
    } else {
        // 如果写入没有跨越页面边界，直接复制数据
        memcpy((char*)区域->映射内存 + 偏移, 数据, 大小);
    }
    
    // 将更改写回目标进程
    kern_return_t kr = vm_write(目标任务, 地址, (vm_offset_t)数据, (mach_msg_type_number_t)大小);
    return (kr == KERN_SUCCESS) ? 0 : -1;
}

int32_t 读内存i32(vm_address_t 地址) {
    int32_t* 值 = (int32_t*)读任意地址(地址, sizeof(int32_t));
    if (!值) {
        return 0;
    }
    int32_t 结果 = *值;
    return 结果;
}

int64_t 读内存i64(vm_address_t 地址) {
    int64_t* 值 = (int64_t*)读任意地址(地址, sizeof(int64_t));
    if (!值) {
        return 0;
    }
    int64_t 结果 = *值;
    return 结果;
}

float 读内存f32(vm_address_t 地址) {
    float* 值 = (float*)读任意地址(地址, sizeof(float));
    if (!值) {
        return 0.0f;
    }
    float 结果 = *值;
    return 结果;
}

double 读内存f64(vm_address_t 地址) {
    double* 值 = (double*)读任意地址(地址, sizeof(double));
    if (!值) {
        return 0.0;
    }
    double 结果 = *值;
    return 结果;
}

int 写内存i32(vm_address_t 地址, int32_t 值) {
    return 写任意地址(地址, &值, sizeof(int32_t));
}

int 写内存i64(vm_address_t 地址, int64_t 值) {
    return 写任意地址(地址, &值, sizeof(int64_t));
}

int 写内存f32(vm_address_t 地址, float 值) {
    return 写任意地址(地址, &值, sizeof(float));
}

int 写内存f64(vm_address_t 地址, double 值) {
    return 写任意地址(地址, &值, sizeof(double));
}

void* 处理内存请求(void* 参数) {
    线程信息* 信息 = (线程信息*)参数;
    
    while (!停止线程) {
        pthread_mutex_lock(&请求互斥锁);
        
        while (等待请求数量 == 0 && !停止线程) {
            pthread_cond_wait(&请求条件变量, &请求互斥锁);
        }
        
        if (停止线程) {
            pthread_mutex_unlock(&请求互斥锁);
            break;
        }
        
        内存请求 请求 = 等待请求[--等待请求数量];
        pthread_mutex_unlock(&请求互斥锁);
        
        if (请求.操作 == 0) {  // 读取操作
            请求.结果 = 读任意地址(请求.地址, 请求.大小);
            memcpy(请求.缓冲区, 请求.结果, 请求.大小);
        } else {  // 写入操作
            写任意地址(请求.地址, 请求.缓冲区, 请求.大小);
        }
    }
    
    return NULL;
}

void 清理未使用页面(time_t 早于) {
    pthread_mutex_lock(&区域互斥锁);
    time_t 当前时间 = time(NULL);
    int i = 0;
    while (i < 缓存区域数量) {
        if (当前时间 - 缓存区域[i].最后访问时间 > 早于) {
            munmap(缓存区域[i].映射内存, 缓存区域[i].映射大小);
            memmove(&缓存区域[i], &缓存区域[i + 1], 
                    (缓存区域数量 - i - 1) * sizeof(内存区域));
            缓存区域数量--;
        } else {
            i++;
        }
    }
    pthread_mutex_unlock(&区域互斥锁);
}

int 初始化内存模块(pid_t pid, 内存模块配置* 配置) {
    if (配置) {
        页面大小 = 配置->页面大小;
        线程数量 = 配置->线程数量;
        最大等待请求数 = 配置->最大等待请求数;
        初始缓存区域数 = 配置->初始缓存区域数;
    }
    
    目标进程ID = pid;
    kern_return_t kr = task_for_pid(mach_task_self(), 目标进程ID, &目标任务);
    if (kr != KERN_SUCCESS) {
        return -1;
    }
    
    最大缓存区域数 = 初始缓存区域数;
    缓存区域 = malloc(最大缓存区域数 * sizeof(内存区域));
    if (!缓存区域) {
        return -1;
    }
    
    线程池 = malloc(线程数量 * sizeof(线程信息));
    if (!线程池) {
        free(缓存区域);
        return -1;
    }
    
    等待请求 = malloc(最大等待请求数 * sizeof(内存请求));
    if (!等待请求) {
        free(缓存区域);
        free(线程池);
        return -1;
    }
    
    内存池 = malloc(内存池大小);
    if (!内存池) {
        free(缓存区域);
        free(线程池);
        free(等待请求);
        return -1;
    }
    
    for (int i = 0; i < 线程数量; i++) {
        线程池[i].id = i;
        if (pthread_create(&线程池[i].线程, NULL, 处理内存请求, &线程池[i]) != 0) {
            for (int j = 0; j < i; j++) {
                pthread_cancel(线程池[j].线程);
                pthread_join(线程池[j].线程, NULL);
            }
            free(缓存区域);
            free(线程池);
            free(等待请求);
            free(内存池);
            return -1;
        }
    }
    
    return 0;
}

void 关闭内存模块() {
    停止线程 = 1;
    pthread_cond_broadcast(&请求条件变量);
    
    for (int i = 0; i < 线程数量; i++) {
        pthread_join(线程池[i].线程, NULL);
    }
    
    for (int i = 0; i < 缓存区域数量; i++) {
        munmap(缓存区域[i].映射内存, 缓存区域[i].映射大小);
    }
    
    free(缓存区域);
    free(线程池);
    free(等待请求);
    free(内存池);
}

void 添加内存请求(内存请求 *请求) {
    pthread_mutex_lock(&请求互斥锁);
    
    while (等待请求数量 >= 最大等待请求数) {
        pthread_cond_wait(&请求条件变量, &请求互斥锁);
    }
    
    等待请求[等待请求数量++] = *请求;
    pthread_cond_signal(&请求条件变量);
    
    pthread_mutex_unlock(&请求互斥锁);
}

vm_address_t 查找内存(const void* 数据, size_t 大小, vm_address_t 开始地址, vm_address_t 结束地址) {
    vm_address_t 当前地址 = 开始地址;
    
    while (当前地址 < 结束地址) {
        void* 内存 = 读任意地址(当前地址, 大小);
        if (!内存) {
            当前地址 += 页面大小;
            continue;
        }
        
        if (memcmp(内存, 数据, 大小) == 0) {
            return 当前地址;
        }
        
        当前地址 += 大小;
    }
    
    return 0;
}

int 读取进程内存(vm_address_t 地址, void* 缓冲区, size_t 大小) {
    内存请求 请求;
    请求.操作 = 0;  // 读取操作
    请求.地址 = 地址;
    请求.缓冲区 = 缓冲区;
    请求.大小 = 大小;
    
    添加内存请求(&请求);
    
    return 0;
}

int 写入进程内存(vm_address_t 地址, const void* 数据, size_t 大小) {
    内存请求 请求;
    请求.操作 = 1;  // 写入操作
    请求.地址 = 地址;
    请求.缓冲区 = (void*)数据;
    请求.大小 = 大小;
    
    添加内存请求(&请求);
    
    return 0;
}

size_t 对齐(size_t 大小, size_t 对齐值) {
    return (大小 + 对齐值 - 1) & ~(对齐值 - 1);
}