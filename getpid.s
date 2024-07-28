.section __TEXT,__text
.globl _main
.globl _get_pid
.p2align 2

// 主程序入口
_main:
    // 打印消息
    mov x0, #1                     // 文件描述符: stdout
    adrp x1, message@PAGE          // 获取 message 的页地址
    add x1, x1, message@PAGEOFF    // 添加页内偏移
    mov x2, #21                    // 消息长度 (包括换行符)
    mov x16, #4                    // syscall: write
    svc #0x80                      // 进行系统调用

    // 调用获取进程 ID 的函数
    bl _get_pid                    // 调用 _get_pid 函数

    // 将进程 ID 转换为字符串并打印
    mov x0, #1                     // 文件描述符: stdout
    adrp x1, pid_message@PAGE      // 获取 pid_message 的页地址
    add x1, x1, pid_message@PAGEOFF// 添加页内偏移
    mov x2, #4                     // "PID:" 的长度
    mov x16, #4                    // syscall: write
    svc #0x80                      // 进行系统调用

    // 打印实际的 PID 数字
    bl _print_number               // 调用打印数字函数

    // 打印换行符
    mov x0, #1                     // 文件描述符: stdout
    adrp x1, newline@PAGE          // 获取 newline 的页地址
    add x1, x1, newline@PAGEOFF    // 添加页内偏移
    mov x2, #1                     // 换行符的长度
    mov x16, #4                    // syscall: write
    svc #0x80                      // 进行系统调用

    // 退出程序
    mov x16, #1                    // syscall: exit
    mov x0, #0                     // 退出码
    svc #0x80                      // 进行系统调用

// 获取进程 ID 的函数
_get_pid:
    mov x16, #20                   // syscall: getpid
    svc #0x80                      // 进行系统调用
    ret                            // 返回，进程 ID 保存在 x0 中

// 打印数字的函数
_print_number:
    mov x1, x0                     // 备份 PID
    add x2, sp, #16                // 指向临时缓冲区
    mov x3, #0                     // 累计字符数
    mov x4, #48                    // 数字 '0' 的 ASCII 值
    mov x7, #10                    // 数字 10

_print_digit:
    udiv x5, x1, x7                // 计算 x1 / 10
    msub x6, x1, x5, x7            // 计算余数 x1 % 10
    add x6, x6, x4                 // 转换为字符
    sub x2, x2, #1                 // 移动指针到缓冲区前一个位置
    strb w6, [x2]                  // 存储字符
    mov x1, x5                     // 更新 x1 为商
    add x3, x3, #1                 // 增加字符数
    cmp x1, #0                     // 检查是否结束
    bne _print_digit               // 如果不是 0，继续

    mov x0, #1                     // 文件描述符: stdout
    mov x1, x2                     // 设置输出字符的指针
    mov x2, x3                     // 设置输出字符的长度
    mov x16, #4                    // syscall: write
    svc #0x80                      // 输出字符

    ret                            // 返回主程序


.section __DATA,__data
message:
    .asciz "Hello, iOS Assembly!\n"
pid_message:
    .asciz "PID:"
newline:
    .asciz "\n"
