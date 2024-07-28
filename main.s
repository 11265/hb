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
    mov x20, sp

    // 读取输入
    mov x0, #0  // stdin
    mov x1, x20 // 缓冲区地址
    mov x2, #256 // 缓冲区大小
    mov x16, #3 // read系统调用
    svc #0x80

    // 检查读取是否成功
    cmp x0, #0
    b.le _error_read

    // 调用sysctl获取进程列表
    sub sp, sp, #32
    mov x21, sp  // 保存mib数组地址

    // 设置mib数组
    mov w0, #1
    str w0, [x21]
    mov w0, #14
    str w0, [x21, #4]
    mov w0, #0
    str w0, [x21, #8]
    str w0, [x21, #12]
    str w0, [x21, #16]
    str w0, [x21, #20]

    // 调用sysctl
    mov x0, x21  // mib数组
    mov w1, #6   // miblen
    mov x2, #0   // oldp
    mov x3, sp   // oldlenp (使用栈上的空间)
    mov x4, #0   // newp
    mov x5, #0   // newlen
    mov x16, #202  // sysctl系统调用
    svc #0x80

    // 检查sysctl调用是否成功
    cmp x0, #0
    b.ne _error_sysctl

    // 打印获取到的大小
    ldr x1, [sp]
    adrp x0, size_msg@PAGE
    add x0, x0, size_msg@PAGEOFF
    bl _printf

    b _exit

_error_sysctl:
    // 打印sysctl错误消息
    adrp x0, error_sysctl_msg@PAGE
    add x0, x0, error_sysctl_msg@PAGEOFF
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
size_msg:
    .asciz "获取到的进程信息大小: %d 字节\n"
error_sysctl_msg:
    .asciz "错误：调用sysctl失败"
error_read_msg:
    .asciz "错误：读取输入失败"
end_msg:
    .asciz "程序执行结束"