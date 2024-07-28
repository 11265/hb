.section __TEXT,__text
.globl _main
.p2align 2

_main:
    // 打印消息
    mov x0, #1                     // 文件描述符：stdout
    adrp x1, message@PAGE          // 获取 message 的页地址
    add x1, x1, message@PAGEOFF    // 添加页内偏移
    mov x2, #21                    // 消息长度 (包括换行符)
    mov x16, #4                    // 系统调用号：write
    svc #0x80                      // 进行系统调用

    // 调用获取进程 ID 的函数
    bl _get_pid

    // 保存 PID
    mov x19, x0                    // 将 PID 保存到 x19 中

    // 打印 "PID: " 消息
    mov x0, #1                     // 文件描述符：stdout
    adrp x1, pid_message@PAGE      // 获取 pid_message 的页地址
    add x1, x1, pid_message@PAGEOFF// 添加页内偏移
    mov x2, #5                     // 消息长度
    mov x16, #4                    // 系统调用号：write
    svc #0x80                      // 进行系统调用

    // 打印实际的 PID 数字
    mov x0, x19                    // 将 PID 传递给打印函数
    bl _print_number               // 调用 _print_number 函数

    // 打印换行符
    mov x0, #1                     // 文件描述符：stdout
    adrp x1, newline@PAGE          // 获取 newline 的页地址
    add x1, x1, newline@PAGEOFF    // 添加页内偏移
    mov x2, #1                     // 消息长度
    mov x16, #4                    // 系统调用号：write
    svc #0x80                      // 进行系统调用

    // 退出程序
    mov x16, #1                    // 系统调用号：exit
    mov x0, #0                     // 退出码
    svc #0x80                      // 进行系统调用

.section __DATA,__data
pid_message:
    .asciz "PID: "                 // PID 前缀消息
newline:
    .asciz "\n"                    // 换行符
