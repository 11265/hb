.section __TEXT,__text
.globl _main
.globl _get_pid
.p2align 2

_main:
    // 打印消息
    mov x0, #1
    adrp x1, message@PAGE
    add x1, x1, message@PAGEOFF
    mov x2, #21
    mov x16, #4
    svc #0x80

    // 调用获取进程 ID 的函数
    bl _get_pid

    // 保存 PID
    mov x19, x0

    // 打印 "PID: " 消息
    mov x0, #1
    adrp x1, pid_message@PAGE
    add x1, x1, pid_message@PAGEOFF
    mov x2, #5
    mov x16, #4
    svc #0x80

    // 打印实际的 PID 数字
    mov x0, x19
    bl _print_number

    // 打印换行符
    mov x0, #1
    adrp x1, newline@PAGE
    add x1, x1, newline@PAGEOFF
    mov x2, #1
    mov x16, #4
    svc #0x80

    // 退出程序
    mov x16, #1
    mov x0, #0
    svc #0x80

_get_pid://获取pid函数
    mov x16, #20     // getpid 系统调用号
    svc #0x80
    ret

_print_number://打印输出函数
    // 为临时缓冲区分配栈空间
    sub sp, sp, #16
    mov x1, sp       // 使用栈顶作为缓冲区
    mov x2, #0       // 字符计数器

_convert_loop:
    mov x3, #10
    udiv x4, x0, x3  // x4 = x0 / 10
    msub x5, x4, x3, x0  // x5 = x0 - (x4 * 10)
    add x5, x5, #48  // 转换为 ASCII
    strb w5, [x1, x2]  // 存储到缓冲区
    add x2, x2, #1   // 增加计数器
    mov x0, x4       // 更新 x0 为商
    cbnz x0, _convert_loop  // 如果不为 0，继续循环

    // 反转字符串
    mov x3, #0       // 开始索引
    sub x4, x2, #1   // 结束索引
_reverse_loop:
    cmp x3, x4
    bge _print_result
    ldrb w5, [x1, x3]
    ldrb w6, [x1, x4]
    strb w6, [x1, x3]
    strb w5, [x1, x4]
    add x3, x3, #1
    sub x4, x4, #1
    b _reverse_loop

_print_result:
    mov x0, #1       // 文件描述符：stdout
    mov x16, #4      // write 系统调用
    svc #0x80

    add sp, sp, #16  // 恢复栈
    ret

.section __DATA,__data
message:
    .asciz "iOS Assembly!\n"
pid_message:
    .asciz "PID: "
newline:
    .asciz "\n"