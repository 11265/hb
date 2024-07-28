.section __DATA,__data
.align 3
buffer:
    .space 256  // 预留256字节的空间用于存储结果
buffer_size:
    .quad 256   // buffer的大小

.section __TEXT,__text
.globl _main
.align 2

_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    // 写入 "Program started" 到标准输出
    mov x0, #1
    adrp x1, start_msg@PAGE
    add x1, x1, start_msg@PAGEOFF
    mov x2, #17  // 消息长度
    mov x16, #4  // write 系统调用
    svc #0x80

    // 调用 sysctlbyname
    adrp x0, sysctl_name@PAGE
    add x0, x0, sysctl_name@PAGEOFF
    adrp x1, buffer@PAGE
    add x1, x1, buffer@PAGEOFF
    adrp x2, buffer_size@PAGE
    add x2, x2, buffer_size@PAGEOFF
    ldr x2, [x2]
    mov x3, #0
    mov x4, #0
    bl _sysctlbyname

    // 检查返回值
    cmp x0, #0
    b.ne _error

    // 写入成功消息
    mov x0, #1
    adrp x1, success_msg@PAGE
    add x1, x1, success_msg@PAGEOFF
    mov x2, #25  // 消息长度
    mov x16, #4  // write 系统调用
    svc #0x80

    // 写入获取的版本字符串
    mov x0, #1
    adrp x1, buffer@PAGE
    add x1, x1, buffer@PAGEOFF
    mov x2, #256  // 最大长度
    mov x16, #4   // write 系统调用
    svc #0x80

    // 正常退出
    mov x0, #0
    b _exit

_error:
    // 写入错误消息
    mov x0, #1
    adrp x1, error_msg@PAGE
    add x1, x1, error_msg@PAGEOFF
    mov x2, #22  // 消息长度
    mov x16, #4  // write 系统调用
    svc #0x80

    mov x0, #1  // 错误退出码

_exit:
    mov x16, #1  // exit 系统调用
    svc #0x80

.section __TEXT,__cstring
start_msg:
    .asciz "Program started\n"
sysctl_name:
    .asciz "kern.osrelease"
success_msg:
    .asciz "sysctlbyname successful:\n"
error_msg:
    .asciz "sysctlbyname failed\n"