.section __TEXT,__text
.globl _main
.globl _get_pid
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

// 获取 PID 的函数
_get_pid:
    mov x16, #20                   // 系统调用号：getpid
    svc #0x80                      // 进行系统调用
    ret                            // 返回，PID 保存在 x0 中

// 打印数字的函数
_print_number:
    // 为临时缓冲区分配栈空间
    sub sp, sp, #16                // 调整栈指针
    mov x1, sp                     // 使用栈顶作为缓冲区
    mov x2, #0                     // 字符计数器

_convert_loop:
    mov x3, #10                    // 设置除数为 10
    udiv x4, x0, x3                // x4 = x0 / 10，计算商
    msub x5, x4, x3, x0            // x5 = x0 - (x4 * 10)，计算余数
    add x5, x5, #48                // 转换为 ASCII 码
    strb w5, [x1, x2]              // 存储到缓冲区
    add x2, x2, #1                 // 增加计数器
    mov x0, x4                     // 更新 x0 为商
    cbnz x0, _convert_loop         // 如果不为 0，继续循环

    // 反转字符串
    mov x3, #0                     // 开始索引
    sub x4, x2, #1                 // 结束索引
_reverse_loop:
    cmp x3, x4                     // 比较开始和结束索引
    bge _print_result              // 如果开始索引大于或等于结束索引，则跳转到打印结果
    ldrb w5, [x1, x3]              // 读取开始索引处的字符
    ldrb w6, [x1, x4]              // 读取结束索引处的字符
    strb w6, [x1, x3]              // 将结束索引处的字符存储到开始索引处
    strb w5, [x1, x4]              // 将开始索引处的字符存储到结束索引处
    add x3, x3, #1                 // 增加开始索引
    sub x4, x4, #1                 // 减少结束索引
    b _reverse_loop                // 继续反转循环

_print_result:
    mov x0, #1                     // 文件描述符：stdout
    mov x16, #4                    // 系统调用号：write
    svc #0x80                      // 输出字符

    add sp, sp, #16                // 恢复栈
    ret                            // 返回主程序

.section __DATA,__data
message:
    .asciz "iOS Assembly!\n"       // 输出的消息
pid_message:
    .asciz "PID: "                 // PID 前缀消息
newline:
    .asciz "\n"                    // 换行符