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

    // 打印 "PID:" 消息
    mov x0, #1
    adrp x1, pid_message@PAGE
    add x1, x1, pid_message@PAGEOFF
    mov x2, #4
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

_get_pid:
    mov x16, #20
    svc #0x80
    ret

_print_number:
    // 为临时缓冲区分配栈空间
    sub sp, sp, #16
    mov x1, x0                     // 备份 PID
    add x2, sp, #15                // 指向临时缓冲区末尾
    mov x3, #0                     // 累计字符数
    mov x4, #48                    // 数字 '0' 的 ASCII 值
    mov x7, #10                    // 数字 10

_print_digit:
    udiv x5, x1, x7                // 计算 x1 / 10
    msub x6, x5, x7, x1            // 计算余数 x1 - (x5 * 10)
    add x6, x6, x4                 // 转换为字符
    strb w6, [x2]                  // 存储字符
    sub x2, x2, #1                 // 移动指针到缓冲区前一个位置
    mov x1, x5                     // 更新 x1 为商
    add x3, x3, #1                 // 增加字符数
    cmp x1, #0                     // 检查是否结束
    bne _print_digit               // 如果不是 0，继续

    add x2, x2, #1                 // 调整指针到第一个数字
    mov x0, #1                     // 文件描述符: stdout
    mov x16, #4                    // syscall: write
    svc #0x80                      // 输出字符

    add sp, sp, #16                // 恢复栈指针
    ret                            // 返回主程序

.section __DATA,__data
message:
    .asciz "Hello, iOS Assembly!\n"
pid_message:
    .asciz "PID:"
newline:
    .asciz "\n"