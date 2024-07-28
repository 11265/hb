.section __TEXT,__text
.globl _main
.align 2

_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    // 打印开始消息
    adrp x0, start_msg@PAGE
    add x0, x0, start_msg@PAGEOFF
    bl _puts

    // 打印输入提示
    adrp x0, input_prompt@PAGE
    add x0, x0, input_prompt@PAGEOFF
    bl _puts

    // 分配输入缓冲区
    sub sp, sp, #256
    mov x20, sp  // x20 保存输入缓冲区地址

    // 读取输入
    mov x0, #0  // stdin
    mov x1, x20 // 缓冲区地址
    mov x2, #256 // 缓冲区大小
    mov x16, #3 // read系统调用
    svc #0x80

    // 检查读取是否成功
    cmp x0, #0
    b.le _error_read

    // 将输入的换行符替换为null
    sub x0, x0, #1
    strb wzr, [x20, x0]

    // 获取进程列表大小
    mov w0, #1  // PROC_ALL_PIDS
    mov x1, #0  // 传递NULL作为buffer
    mov x2, #0  // buffersize为0
    mov x3, #0  // 不需要实际数量
    bl _proc_listpids

    // 检查返回值
    cmp x0, #0
    b.le _error_proc_listpids

    // 保存所需大小
    mov x21, x0

    // 分配内存
    mov x0, x21
    bl _malloc
    mov x22, x0  // x22 保存分配的内存地址

    // 获取进程列表
    mov w0, #1  // PROC_ALL_PIDS
    mov x1, x22  // buffer
    mov x2, x21  // buffersize
    mov x3, #0   // 不需要实际数量
    bl _proc_listpids

    // 计算进程数量
    udiv x23, x0, #4  // x23 保存进程数量

    // 打印进程数量
    mov x1, x23
    adrp x0, proc_count_msg@PAGE
    add x0, x0, proc_count_msg@PAGEOFF
    bl _printf

    // 遍历进程
    mov x24, #0  // 进程计数器

_proc_loop:
    cmp x24, x23
    b.ge _end_loop

    // 获取当前PID
    ldr w25, [x22, x24, lsl #2]  // x25 保存当前PID

    // 获取进程名
    sub sp, sp, #32
    mov x0, x25  // pid
    mov x1, #1   // PROC_PIDTBSDINFO
    mov x2, #0   // 偏移量
    mov x3, sp   // buffer
    mov x4, #32  // buffer size
    bl _proc_pidinfo

    // 检查是否成功获取进程名
    cmp x0, #0
    b.le _next_proc

    // 比较进程名
    mov x0, sp
    add x0, x0, #16  // 进程名在结构体中的偏移
    mov x1, x20  // 用户输入
    bl _strcmp

    cmp x0, #0
    b.ne _next_proc

    // 找到匹配的进程，打印PID
    mov x1, x25
    adrp x0, found_proc_msg@PAGE
    add x0, x0, found_proc_msg@PAGEOFF
    bl _printf

_next_proc:
    add x24, x24, #1
    b _proc_loop

_end_loop:
    // 释放内存
    mov x0, x22
    bl _free

    b _exit

_error_proc_listpids:
    // 打印proc_listpids错误消息
    adrp x0, error_proc_listpids_msg@PAGE
    add x0, x0, error_proc_listpids_msg@PAGEOFF
    bl _puts
    b _exit

_error_read:
    // 打印读取错误消息
    adrp x0, error_read_msg@PAGE
    add x0, x0, error_read_msg@PAGEOFF
    bl _puts

_exit:
    // 打印结束消息
    adrp x0, end_msg@PAGE
    add x0, x0, end_msg@PAGEOFF
    bl _puts

    mov x0, #0
    mov x16, #1 // exit系统调用
    svc #0x80

.section __TEXT,__cstring
start_msg:
    .asciz "程序开始执行"
input_prompt:
    .asciz "请输入要查找的进程名称: "
proc_count_msg:
    .asciz "总进程数: %d\n"
found_proc_msg:
    .asciz "找到匹配的进程，PID: %d\n"
error_proc_listpids_msg:
    .asciz "错误：调用proc_listpids失败"
error_read_msg:
    .asciz "错误：读取输入失败"
end_msg:
    .asciz "程序执行结束"