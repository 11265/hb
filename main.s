.section __DATA,__data
.align 3
pids:
    .space 8*1000  // 假设最多1000个进程，每个进程ID占8字节
max_pids:
    .quad 1000
buffer_size:
    .quad 8000

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
    adrp x1, buffer_size@PAGE
    add x1, x1, buffer_size@PAGEOFF
    ldr x1, [x1]
    bl _proc_listallpids

    // 检查返回值
    cmp x0, #0
    b.le _error_proc_list
    adrp x1, buffer_size@PAGE
    add x1, x1, buffer_size@PAGEOFF
    ldr x1, [x1]
    cmp x0, x1
    b.gt _error_too_many_bytes

    // 保存返回的字节数
    mov x19, x0

    // 打印原始返回值（字节数）
    adrp x0, raw_return_msg@PAGE
    add x0, x0, raw_return_msg@PAGEOFF
    mov x1, x19
    bl _printf

    // 计算实际进程数量（字节数除以8）
    lsr x19, x19, #3  // 右移3位，相当于除以8

    // 检查进程数量是否合理
    adrp x1, max_pids@PAGE
    add x1, x1, max_pids@PAGEOFF
    ldr x1, [x1]
    cmp x19, x1
    b.gt _error_too_many

    // 打印进程数量
    adrp x0, count_msg@PAGE
    add x0, x0, count_msg@PAGEOFF
    mov x1, x19
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
    mov x22, #0  // 循环计数器
    adrp x23, pids@PAGE
    add x23, x23, pids@PAGEOFF  // x23 = pids数组起始地址

_loop:
    cmp x22, x19
    b.ge _not_found

    // 获取当前进程ID
    ldr x24, [x23, x22, lsl #3]

    // 为进程路径分配栈空间
    sub sp, sp, #1024
    mov x25, sp

    // 调用proc_pidpath获取进程路径
    mov x0, x24  // 进程ID
    mov x1, x25  // 缓冲区
    mov x2, #1024  // 缓冲区大小
    bl _proc_pidpath

    // 检查返回值
    cmp x0, #0
    b.le _continue_loop

    // 获取进程名（最后一个'/'之后的部分）
    mov x0, x25
    mov x1, #47  // '/'的ASCII码
    bl _strrchr
    cmp x0, #0
    b.eq _continue_loop
    add x0, x0, #1  // 移过'/'

    // 比较进程名
    mov x1, x20  // 用户输入的进程名
    bl _strcmp
    cmp x0, #0
    b.ne _continue_loop

    // 找到匹配的进程，打印PID
    adrp x0, found_msg@PAGE
    add x0, x0, found_msg@PAGEOFF
    mov x1, x24
    bl _printf
    b _exit

_continue_loop:
    // 恢复栈
    add sp, sp, #1024

    // 增加循环计数器
    add x22, x22, #1
    b _loop

_not_found:
    adrp x0, not_found_msg@PAGE
    add x0, x0, not_found_msg@PAGEOFF
    bl _printf
    b _exit

_error_proc_list:
    // 打印错误消息
    adrp x0, error_proc_list_msg@PAGE
    add x0, x0, error_proc_list_msg@PAGEOFF
    bl _printf
    b _exit

_error_too_many_bytes:
    // 打印错误消息：返回的字节数过多
    adrp x0, error_too_many_bytes_msg@PAGE
    add x0, x0, error_too_many_bytes_msg@PAGEOFF
    mov x1, x0
    bl _printf
    b _exit

_error_too_many:
    // 打印错误消息：进程数量过多
    adrp x0, error_too_many_msg@PAGE
    add x0, x0, error_too_many_msg@PAGEOFF
    mov x1, x19
    adrp x2, max_pids@PAGE
    add x2, x2, max_pids@PAGEOFF
    ldr x2, [x2]
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
raw_return_msg:
    .asciz "proc_listallpids 原始返回值（字节数）: %d\n"
error_proc_list_msg:
    .asciz "获取进程列表失败\n"
error_too_many_bytes_msg:
    .asciz "错误：返回的字节数超过了缓冲区大小\n"
error_too_many_msg:
    .asciz "错误：进程数量 (%d) 超过最大限制 (%d)\n"
error_read_input_msg:
    .asciz "错误：读取用户输入失败\n"
count_msg:
    .asciz "进程数量: %d\n"
input_prompt:
    .asciz "请输入要查找的进程名称: "
found_msg:
    .asciz "找到匹配的进程，PID: %d\n"
not_found_msg:
    .asciz "未找到匹配的进程\n"
end_msg:
    .asciz "程序执行结束\n"