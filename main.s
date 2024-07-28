.section __DATA,__data
.align 3
pids:
    .space 8*1000  // 假设最多1000个进程，每个进程ID占8字节
max_pids:
    .quad 1000
buffer_size:
    .quad 8000
mib:
    .long 1, 14, 0, 0  // CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0

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

    // 调用sysctl获取所有进程ID
    sub sp, sp, #16
    mov x0, sp
    adrp x1, buffer_size@PAGE
    add x1, x1, buffer_size@PAGEOFF
    ldr x1, [x1]
    str x1, [x0]  // 存储buffer_size到栈上

    adrp x0, mib@PAGE
    add x0, x0, mib@PAGEOFF
    mov x1, #4  // mib数组长度
    mov x2, sp  // oldp (buffer size pointer)
    mov x3, sp  // oldlenp
    adrp x4, pids@PAGE
    add x4, x4, pids@PAGEOFF
    mov x5, #0  // newp
    mov x6, #0  // newlen
    mov x16, #202  // sysctl系统调用号
    svc #0x80

    // 检查返回值
    cmp x0, #0
    b.ne _error_proc_list

    // 计算进程数量
    ldr x19, [sp]
    lsr x19, x19, #3  // 除以8，得到进程数量

    // 打印进程数量
    adrp x0, count_msg@PAGE
    add x0, x0, count_msg@PAGEOFF
    mov x1, x19
    bl _printf

    // 恢复栈
    add sp, sp, #16

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

    // 为进程名称分配栈空间
    sub sp, sp, #32
    mov x25, sp

    // 调用proc_name获取进程名称
    mov x0, x24  // 进程ID
    mov x1, x25  // 缓冲区
    mov x2, #32  // 缓冲区大小
    bl _proc_name

    // 检查返回值
    cmp x0, #0
    b.eq _continue_loop

    // 比较进程名
    mov x0, x25  // 获取到的进程名
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
    add sp, sp, #32

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
error_proc_list_msg:
    .asciz "获取进程列表失败\n"
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