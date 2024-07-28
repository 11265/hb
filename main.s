.section __DATA,__data
.align 3
buffer:
    .space 1024*1024  // 1MB用于进程信息的缓冲区
buffer_size:
    .quad 1024*1024
pids:
    .space 4*1024  // 假设最多1024个进程，每个进程ID占4字节

.section __TEXT,__text
.globl _main
.align 2

_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    // 调用proc_listallpids获取所有进程ID
    adrp x0, pids@PAGE
    add x0, x0, pids@PAGEOFF
    mov x1, #4096  // 4*1024
    bl _proc_listallpids

    // 检查返回值
    cmp x0, #0
    b.le _error

    // 保存进程数量
    mov x19, x0

    // 打印进程数量
    adrp x0, count_msg@PAGE
    add x0, x0, count_msg@PAGEOFF
    mov x1, x19
    bl _printf

    // 初始化循环
    mov x20, #0  // 循环计数器
    adrp x21, pids@PAGE
    add x21, x21, pids@PAGEOFF  // x21 = pids数组起始地址

_loop:
    cmp x20, x19
    b.ge _exit

    // 获取当前进程ID
    ldr w22, [x21, x20, lsl #2]

    // 为进程路径分配栈空间
    sub sp, sp, #1024
    mov x23, sp

    // 调用proc_pidpath获取进程路径
    mov x0, x22  // 进程ID
    mov x1, x23  // 缓冲区
    mov x2, #1024  // 缓冲区大小
    bl _proc_pidpath

    // 检查返回值
    cmp x0, #0
    b.le _skip_print

    // 打印进程ID和路径
    adrp x0, process_info_msg@PAGE
    add x0, x0, process_info_msg@PAGEOFF
    mov x1, x22
    mov x2, x23
    bl _printf

_skip_print:
    // 恢复栈
    add sp, sp, #1024

    // 增加循环计数器
    add x20, x20, #1
    b _loop

_error:
    // 打印错误消息
    adrp x0, error_msg@PAGE
    add x0, x0, error_msg@PAGEOFF
    mov x1, x0
    bl _printf

_exit:
    mov x0, #0
    mov x16, #1  // exit系统调用
    svc #0x80

.section __TEXT,__cstring
error_msg:
    .asciz "获取进程列表失败\n"
count_msg:
    .asciz "进程数量: %d\n"
process_info_msg:
    .asciz "进程ID: %d, 路径: %s\n"