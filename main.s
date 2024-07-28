.section __DATA,__data
.align 3
pids:
    .space 4*1024  // 假设最多1024个进程，每个进程ID占4字节
max_pids:
    .long 1024

.section __TEXT,__text
.globl _main
.align 2

_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    // 打印开始消息
    adrp x0, start_msg@PAGE
    add x0, x0, start_msg@PAGEOFF
    bl _printf

    // 调用proc_listallpids获取所有进程ID
    adrp x0, pids@PAGE
    add x0, x0, pids@PAGEOFF
    mov x1, #4096  // 4*1024
    bl _proc_listallpids

    // 检查返回值
    cmp x0, #0
    b.le _error_proc_list

    // 保存返回的字节数
    mov x19, x0

    // 打印原始返回值（字节数）
    adrp x0, raw_return_msg@PAGE
    add x0, x0, raw_return_msg@PAGEOFF
    mov x1, x19
    bl _printf

    // 计算实际进程数量（字节数除以4）
    mov x20, #4
    udiv x19, x19, x20

    // 检查进程数量是否合理
    adrp x1, max_pids@PAGE
    add x1, x1, max_pids@PAGEOFF
    ldr w1, [x1]
    cmp x19, x1
    b.gt _error_too_many

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

    // 打印当前处理的进程ID
    adrp x0, processing_pid_msg@PAGE
    add x0, x0, processing_pid_msg@PAGEOFF
    mov x1, x22
    bl _printf

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

_error_proc_list:
    // 打印错误消息
    adrp x0, error_proc_list_msg@PAGE
    add x0, x0, error_proc_list_msg@PAGEOFF
    bl _printf
    b _exit

_error_too_many:
    // 打印错误消息：进程数量过多
    adrp x0, error_too_many_msg@PAGE
    add x0, x0, error_too_many_msg@PAGEOFF
    mov x1, x19
    adrp x2, max_pids@PAGE
    add x2, x2, max_pids@PAGEOFF
    ldr w2, [x2]
    bl _printf
    b _exit

_exit:
    // 打印结束消息
    adrp x0, end_msg@PAGE
    add x0, x0, end_msg@PAGEOFF
    bl _printf

    mov x0, #0
    mov x16, #1  // exit系统调用
    svc #0x80

.section __TEXT,__cstring
start_msg:
    .asciz "程序开始执行\n"
raw_return_msg:
    .asciz "proc_listallpids 原始返回值（字节数）: %d\n"
error_proc_list_msg:
    .asciz "获取进程列表失败\n"
error_too_many_msg:
    .asciz "错误：进程数量 (%d) 超过最大限制 (%d)\n"
count_msg:
    .asciz "进程数量: %d\n"
processing_pid_msg:
    .asciz "正在处理进程ID: %d\n"
process_info_msg:
    .asciz "进程ID: %d, 路径: %s\n"
end_msg:
    .asciz "程序执行结束\n"