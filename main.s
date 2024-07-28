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

    // 执行ps命令
    adrp x0, ps_command@PAGE
    add x0, x0, ps_command@PAGEOFF
    adrp x1, read_mode@PAGE
    add x1, x1, read_mode@PAGEOFF
    bl _popen

    // 检查popen是否成功
    cmp x0, #0
    b.eq _error_popen

    // 保存文件指针
    mov x19, x0

    // 为读取缓冲区分配栈空间
    sub sp, sp, #1024
    mov x21, sp

_read_loop:
    // 读取一行
    mov x0, x21
    mov x1, #1024
    mov x2, x19
    bl _fgets

    // 检查是否读到EOF
    cmp x0, #0
    b.eq _not_found

    // 检查是否包含目标进程名
    mov x0, x21
    mov x1, x20
    bl _strstr
    cmp x0, #0
    b.eq _read_loop

    // 找到匹配的进程，提取PID
    mov x0, x21
    bl _atoi

    // 打印找到的PID
    mov x1, x0
    adrp x0, found_msg@PAGE
    add x0, x0, found_msg@PAGEOFF
    bl _printf
    b _cleanup

_not_found:
    adrp x0, not_found_msg@PAGE
    add x0, x0, not_found_msg@PAGEOFF
    bl _puts

_cleanup:
    // 关闭文件
    mov x0, x19
    bl _pclose

    b _exit

_error_popen:
    adrp x0, error_popen_msg@PAGE
    add x0, x0, error_popen_msg@PAGEOFF
    bl _puts
    b _exit

_error_read_input:
    adrp x0, error_read_input_msg@PAGE
    add x0, x0, error_read_input_msg@PAGEOFF
    bl _puts

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
input_prompt:
    .asciz "请输入要查找的进程名称: "
input_received_msg:
    .asciz "收到用户输入: %s\n"
ps_command:
    .asciz "ps -e"
read_mode:
    .asciz "r"
found_msg:
    .asciz "找到匹配的进程，PID: %d\n"
not_found_msg:
    .asciz "未找到匹配的进程"
error_popen_msg:
    .asciz "错误：无法执行ps命令"
error_read_input_msg:
    .asciz "错误：读取用户输入失败"
end_msg:
    .asciz "程序执行结束"