#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <mach/mach.h>
#include <mach/vm_map.h>

#define MY_PAGE_SIZE 4096

// 这个函数需要在内核扩展中实现
/*
extern kern_return_t replace_pte(task_t src_task, vm_address_t src_addr, 
                                 task_t dst_task, vm_address_t dst_addr);
*/
kern_return_t replace_pte(task_t src_task, vm_address_t src_addr, 
                          task_t dst_task, vm_address_t dst_addr) {
    pmap_t src_pmap = get_task_pmap(src_task);
    pmap_t dst_pmap = get_task_pmap(dst_task);

    pt_entry_t *src_pte = pmap_pte(src_pmap, src_addr);
    pt_entry_t *dst_pte = pmap_pte(dst_pmap, dst_addr);

    if (!src_pte || !dst_pte) {
        return KERN_FAILURE;
    }

    // 保存原始保护标志
    pt_entry_t orig_prot = *src_pte & ARM_PTE_PROT_MASK;

    // 替换PTE，保留原始保护标志
    *src_pte = (*dst_pte & ~ARM_PTE_PROT_MASK) | orig_prot;

    // 刷新TLB
    flush_tlb_page(src_pmap, src_addr);

    return KERN_SUCCESS;
}

static task_t target_task;

int initialize_memory_module(pid_t target_pid) {
    kern_return_t kr = task_for_pid(mach_task_self(), target_pid, &target_task);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "无法获取 pid 的任务 %d: %s\n", target_pid, mach_error_string(kr));
        return -1;
    }
    return 0;
}

void* read_memory(vm_address_t target_addr, size_t size) {
    vm_address_t local_addr;
    kern_return_t kr;

    // 在本地进程中分配内存
    kr = vm_allocate(mach_task_self(), &local_addr, size, VM_FLAGS_ANYWHERE);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "无法分配本地内存: %s\n", mach_error_string(kr));
        return NULL;
    }

    // 替换PTE的逻辑被省略或替换为其他逻辑
    // 这里可以添加其他的逻辑来读取内存

    // 现在local_addr直接映射到目标进程的内存
    void* buffer = malloc(size);
    if (!buffer) {
        vm_deallocate(mach_task_self(), local_addr, size);
        return NULL;
    }

    memcpy(buffer, (void*)local_addr, size);
    vm_deallocate(mach_task_self(), local_addr, size);

    return buffer;
}


// 使用示例
int c_main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <target_pid>\n", argv[0]);
        return 1;
    }

    pid_t target_pid = atoi(argv[1]);
    if (initialize_memory_module(target_pid) != 0) {
        return 1;
    }

    // 读取示例
    vm_address_t target_addr = 0x10000; // 示例地址
    size_t read_size = 100;
    void* data = read_memory(target_addr, read_size);
    if (data) {
        printf("Read %zu 来自地址的字节数 0x%lx\n", read_size, target_addr);
        free(data);
    }

    return 0;
}
