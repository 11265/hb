.section __DATA,__data
.align 3
max_pid:
    .quad 10000000  // 增加最大PID范围

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

    // 获取用户输入的进程名
    sub sp, sp, #256
    mov x20, sp  // 保存进程名缓冲区地址

    adrp x0, input_prompt@PAGE
    add x0, x0, input_prompt@PAGEOFF
    bl _puts

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

    // 打印用户输入
    adrp x0, input_received_msg@PAGE
    add x0, x0, input_received_msg@PAGEOFF
    mov x1, x20
    bl _printf

    // 初始化循环
    mov x22, #1  // 从PID 1开始

_loop:
    adrp x0, max_pid@PAGE
    add x0, x0, max_pid@PAGEOFF
    ldr x0, [x0]
    cmp x22, x0
    b.gt _not_found

    // 为进程信息分配栈空间
    sub sp, sp, #1024
    mov x23, sp

    // 调用sysctl获取进程信息
    mov x0, sp  // mib数组
    mov x1, #4  // mib长度
    mov x2, x23  // 输出缓冲区
    mov x3, #1024  // 缓冲区大小
    mov x4, #0  // 旧长度
    mov x5, #0  // 新长度

    // 设置mib: [CTL_KERN, KERN_PROC, KERN_PROC_PID, current_pid]
    mov w6, #1
    str w6, [x0], #4
    mov w6, #14
    str w6, [x0], #4
    mov w6, #1
    str w6, [x0], #4
    str w22, [x0]

    mov x16, #202  // sysctl系统调用
    svc #0x80

    // 检查返回值
    cmp x0, #0
    b.ne _continue_loop

    // 比较进程名
    add x0, x23, #0x1A0  // 进程名在结构体中的偏移
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
    add sp, sp, #1024

    // 增加PID
    add x22, x22, #1
    b _loop

_not_found:
    adrp x0, not_found_msg@PAGE
    add x0, x0, not_found_msg@PAGEOFF
    bl _puts
    b _exit

_error_read_input:
    // 打印错误消息：读取输入失败
    adrp x0, error_read_input_msg@PAGE
    add x0, x0, error_read_input_msg@PAGEOFF
    bl _puts
    b _exit

_exit:
    // 打印结束消息
    adrp x0, end_msg@PAGE
    add x0, x0, end_msg@PAGEOFF
    bl _puts

    mov x0, #0
    mov x16, #1  // exit系统调用
    svc #0x80

.section __TEXT,__cstring
start_msg:
    .asciz "程序开始执行"
error_read_input_msg:
    .asciz "错误：读取用户输入失败"
input_prompt:
    .asciz "请输入要查找的进程名称: "
input_received_msg:
    .asciz "收到用户输入: %s\n"
found_msg:
    .asciz "找到匹配的进程，PID: %d\n"
not_found_msg:
    .asciz "未找到匹配的进程"
end_msg:
    .asciz "程序执行结束"