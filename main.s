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

    // 打印ps输出
    adrp x0, ps_output_msg@PAGE
    add x0, x0, ps_output_msg@PAGEOFF
    bl _puts

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
    b.eq _end_read

    // 打印读取的行
    mov x0, x21
    bl _printf

    b _read_loop

_end_read:
    // 关闭文件
    mov x0, x19
    bl _pclose

    b _exit

_error_popen:
    // 打印popen错误消息
    adrp x0, error_popen_msg@PAGE
    add x0, x0, error_popen_msg@PAGEOFF
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
ps_command:
    .asciz "ps -e"
read_mode:
    .asciz "r"
ps_output_msg:
    .asciz "ps命令输出："
error_popen_msg:
    .asciz "错误：无法执行ps命令"
error_read_msg:
    .asciz "错误：读取输入失败"
end_msg:
    .asciz "程序执行结束"