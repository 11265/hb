.section __DATA,__data
.align 3
max_pid:
    .quad 99999  // 假设最大PID为99999

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

    // 获取用户输入的进程名
    sub sp, sp, #256
    mov x20, sp  // 保存进程名缓冲区地址

    adrp x0, input_prompt@PAGE
    add x0, x0, input_prompt@PAGEOFF
    bl _printf

    mov x0, #0  // stdin
    mov x1, x20  // 缓冲区地址
    mov x2, #256  // 缓冲区大小
    mov x16, #3  // read系统调用
    svc #0x80

    // 检查读取是否成功
    cmp x0, #0
    b.le _error_read_input

    // 移除换行符
    mov x21, x0  // 保存读取的字节数
    sub x21, x21, #1
    strb wzr, [x20, x21]

    // 初始化循环
    mov x22, #1  // 从PID 1开始

_loop:
    adrp x0, max_pid@PAGE
    add x0, x0, max_pid@PAGEOFF
    ldr x0, [x0]
    cmp x22, x0
    b.gt _not_found

    // 为进程名称分配栈空间
    sub sp, sp, #32
    mov x23, sp

    // 调用proc_name获取进程名称
    mov x0, x22  // 当前PID
    mov x1, x23  // 缓冲区
    mov x2, #32  // 缓冲区大小
    bl _proc_name

    // 检查返回值
    cmp x0, #0
    b.eq _continue_loop

    // 比较进程名
    mov x0, x23  // 获取到的进程名
    mov x1, x20  // 用户输入的进程名
    bl _strcmp
    cmp x0, #0
    b.ne _continue_loop

    // 找到匹配的进程，打印PID
    adrp x0, found_msg@PAGE
    add x0, x0, found_msg@PAGEOFF
    mov x1, x22
    bl _printf
    b _exit

_continue_loop:
    // 恢复栈
    add sp, sp, #32

    // 增加PID
    add x22, x22, #1
    b _loop

_not_found:
    adrp x0, not_found_msg@PAGE
    add x0, x0, not_found_msg@PAGEOFF
    bl _printf
    b _exit

_error_read_input:
    // 打印错误消息：读取输入失败
    adrp x0, error_read_input_msg@PAGE
    add x0, x0, error_read_input_msg@PAGEOFF
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
error_read_input_msg:
    .asciz "错误：读取用户输入失败\n"
input_prompt:
    .asciz "请输入要查找的进程名称: "
found_msg:
    .asciz "找到匹配的进程，PID: %d\n"
not_found_msg:
    .asciz "未找到匹配的进程\n"
end_msg:
    .asciz "程序执行结束\n"