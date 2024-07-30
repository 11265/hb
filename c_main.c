// c_main.c

#include "内存模块.h"
#include "查找进程.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>

#define TARGET_PROCESS_NAME "pvz"
#define START_ADDRESS 0x100000000ULL  // 从 4GB 开始
#define END_ADDRESS   0x200000000ULL  // 到 8GB 结束
#define THREAD_COUNT  4
#define BATCH_SIZE    (1024 * 1024)   // 1MB 批量读取
#define MAX_RESULTS   1000            // 最大结果数

volatile sig_atomic_t keep_running = 1;

typedef struct {
    vm_address_t address;
    int32_t value;
} ScanResult;

typedef struct {
    vm_address_t start;
    vm_address_t end;
    ScanResult* results;
    int result_count;
    pthread_mutex_t* mutex;
} ScanRange;

void intHandler(int dummy) {
    keep_running = 0;
}

void* scan_thread(void* arg) {
    ScanRange* range = (ScanRange*)arg;
    vm_address_t current = range->start;
    int32_t* buffer = malloc(BATCH_SIZE);

    while (current < range->end && keep_running) {
        size_t batch_size = (range->end - current < BATCH_SIZE) ? range->end - current : BATCH_SIZE;
        
        if (读任意地址(current, buffer, batch_size) == 0) {
            for (size_t i = 0; i < batch_size / sizeof(int32_t); i++) {
                if (buffer[i] > 100) {
                    pthread_mutex_lock(range->mutex);
                    if (range->result_count < MAX_RESULTS) {
                        range->results[range->result_count].address = current + i * sizeof(int32_t);
                        range->results[range->result_count].value = buffer[i];
                        range->result_count++;
                    }
                    pthread_mutex_unlock(range->mutex);
                }
            }
        }

        current += batch_size;

        // 每扫描 100MB 输出一次进度
        if (current % (100 * 1024 * 1024) == 0) {
            printf("线程 %lu 进度: 0x%llx (%.2f%%)\n", 
                   (unsigned long)pthread_self(), 
                   (unsigned long long)current, 
                   (float)(current - range->start) / (range->end - range->start) * 100);
        }
    }

    free(buffer);
    return NULL;
}

void 扫描内存范围() {
    printf("开始多线程扫描内存范围 0x%llx 到 0x%llx\n", START_ADDRESS, END_ADDRESS);

    signal(SIGINT, intHandler);

    pthread_t threads[THREAD_COUNT];
    ScanRange ranges[THREAD_COUNT];
    vm_address_t range_size = (END_ADDRESS - START_ADDRESS) / THREAD_COUNT;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    ScanResult* all_results = malloc(MAX_RESULTS * sizeof(ScanResult) * THREAD_COUNT);

    for (int i = 0; i < THREAD_COUNT; i++) {
        ranges[i].start = START_ADDRESS + i * range_size;
        ranges[i].end = (i == THREAD_COUNT - 1) ? END_ADDRESS : ranges[i].start + range_size;
        ranges[i].results = &all_results[i * MAX_RESULTS];
        ranges[i].result_count = 0;
        ranges[i].mutex = &mutex;
        pthread_create(&threads[i], NULL, scan_thread, &ranges[i]);
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    if (!keep_running) {
        printf("\n扫描被用户中断\n");
    } else {
        printf("内存扫描完成\n");
    }

    // 打印结果
    printf("\n扫描结果：\n");
    int total_results = 0;
    for (int i = 0; i < THREAD_COUNT; i++) {
        for (int j = 0; j < ranges[i].result_count; j++) {
            printf("地址: 0x%llx, 值: %d\n", 
                   (unsigned long long)ranges[i].results[j].address, 
                   ranges[i].results[j].value);
            total_results++;
        }
    }
    printf("共找到 %d 个结果\n", total_results);

    free(all_results);
}

int c_main() {
    pid_t target_pid = get_pid_by_name(TARGET_PROCESS_NAME);
    if (target_pid == -1) {
        printf("无法找到目标进程: %s\n", TARGET_PROCESS_NAME);
        return 1;
    }

    printf("找到目标进程 %s, PID: %d\n", TARGET_PROCESS_NAME, target_pid);

    if (初始化内存模块(target_pid) != 0) {
        printf("初始化内存模块失败\n");
        return 1;
    }

    printf("内存模块初始化成功\n");

    printf("开始执行内存扫描。按 Ctrl+C 随时中断。\n");
    扫描内存范围();

    关闭内存模块();
    printf("内存模块已关闭\n");

    return 0;
}