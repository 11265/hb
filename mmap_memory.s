.equ TARGET_PID, 1234  // 将这里的1234替换为目标进程的实际PID
.global _main
.align 2


_main:
    // Save link register
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    // Call our mmap_and_read_memory function
    bl _mmap_and_read_memory

    // Exit program
    mov x0, #0
    ldp x29, x30, [sp], #16
    ret

.global _mmap_and_read_memory
_mmap_and_read_memory:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    
    // 初始化变量
    mov x20, #TARGET_PID      // 使用预定义的目标进程PID
    mov x21, #0               // 目标内存地址，设为0让系统选择合适的地址
    mov x22, #4096            // 要映射的内存大小，这里设置为4KB
    
    // 获取当前进程的任务端口
    bl _task_self_trap
    mov x19, x0               // 保存当前任务端口
    
    // 获取目标进程的任务端口
    mov x0, x19               // 当前任务端口
    mov x1, x20               // 目标进程PID
    mov x2, sp                // 用于存储返回的目标任务端口
    bl _task_for_pid
    cbnz x0, _exit            // 如果task_for_pid返回非零，则失败
    ldr x25, [sp]             // 加载目标任务端口
    
    // 使用mmap映射目标进程内存
    mov x0, x21               // 映射的起始地址
    mov x1, x22               // 映射的大小
    mov x2, #3                // 映射的保护标志 (PROT_READ | PROT_WRITE)
    mov x3, #1                // 映射的标志 (MAP_SHARED)
    mov x4, x25               // 文件描述符（在这里是目标任务端口）
    mov x5, #0                // 偏移量
    bl _mmap                  // 调用mmap系统调用
    cbz x0, _exit             // 如果mmap返回零，则失败
    
    // 映射成功，x0保存映射区域的地址
    mov x26, x0               // 保存映射区域的起始地址

    // 读取内存测试
    // 读取前8个字节（64位）到x27
    ldr x27, [x26]
    // 读取接下来的4个字节（32位）到w28
    ldr w28, [x26, #8]
    // 读取接下来的2个字节（16位）到w29的低16位
    ldrh w29, [x26, #12]
    // 读取接下来的1个字节（8位）到w30的低8位
    ldrb w30, [x26, #14]

    // 在这里，x27, w28, w29, w30 中包含了读取的数据
    // 您可以根据需要对这些数据进行进一步处理或检查
    ret

_exit:
    // 解除内存映射
    mov x0, x26               // 映射的地址
    mov x1, x22               // 映射的大小
    bl _munmap                // 调用munmap系统调用

    // 恢复寄存器状态并返回
    ldp x29, x30, [sp], #16
    ret

// task_self_trap 获取当前任务端口
_task_self_trap:
    mov x16, #330             // syscall number for task_self_trap
    svc #0                    // perform syscall
    ret

// task_for_pid 获取目标进程的任务端口
_task_for_pid:
    mov x16, #45              // syscall number for task_for_pid
    svc #0                    // perform syscall
    ret

// mmap 映射内存
_mmap:
    mov x16, #197             // syscall number for mmap
    svc #0                    // perform syscall
    ret

// munmap 解除内存映射
_munmap:
    mov x16, #73              // syscall number for munmap
    svc #0                    // perform syscall
    ret