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

    // 打印提示消息
    adrp x0, prompt_msg@PAGE
    add x0, x0, prompt_msg@PAGEOFF
    bl _printf

    // 尝试读取用户输入
    sub sp, sp, #256
    mov x0, #0  // stdin
    mov x1, sp  // 缓冲区地址
    mov x2, #256  // 缓冲区大小
    mov x16, #3  // read系统调用
    svc #0x80

    // 检查读取是否成功
    cmp x0, #0
    b.le _read_error

    // 打印读取成功消息
    adrp x0, read_success_msg@PAGE
    add x0, x0, read_success_msg@PAGEOFF
    bl _printf
    b _exit

_read_error:
    // 打印读取错误消息
    adrp x0, read_error_msg@PAGE
    add x0, x0, read_error_msg@PAGEOFF
    bl _printf

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
prompt_msg:
    .asciz "请输入任意内容: "
read_success_msg:
    .asciz "成功读取用户输入\n"
read_error_msg:
    .asciz "读取用户输入失败\n"
end_msg:
    .asciz "程序执行结束\n"