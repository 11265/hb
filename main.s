.section __TEXT,__text
.globl _main
.align 2

_main:
    // 保存链接寄存器
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    // 写入 "Hello, World!" 到标准输出
    mov x0, #1           // 文件描述符 1 = 标准输出
    adrp x1, message@PAGE
    add x1, x1, message@PAGEOFF
    mov x2, #14          // 消息长度
    mov x16, #4          // 系统调用号 4 = write
    svc #0x80

    // 检查系统调用是否成功
    cmp x0, #0
    b.lt _error

    // 正常退出
    mov x0, #0
    mov x16, #1          // 系统调用号 1 = exit
    svc #0x80

_error:
    // 处理错误
    mov x0, #1
    mov x16, #1          // 系统调用号 1 = exit
    svc #0x80

.section __TEXT,__cstring
message:
    .ascii "Hello, World!\n"