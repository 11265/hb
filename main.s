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

    // 打印读取成功消息
    adrp x0, read_success_msg@PAGE
    add x0, x0, read_success_msg@PAGEOFF
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
    .asciz "请输入内容: "
read_success_msg:
    .asciz "成功读取输入"
error_read_msg:
    .asciz "读取输入时发生错误"
end_msg:
    .asciz "程序执行结束"