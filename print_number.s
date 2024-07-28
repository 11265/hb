.section __TEXT,__text
.globl _print_number
.p2align 2

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
