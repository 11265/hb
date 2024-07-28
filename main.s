.section __DATA,__data
.align 3
mib:
    .long 1, 2  // CTL_KERN, KERN_OSRELEASE
buffer:
    .space 256
buffer_size:
    .quad 256

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
    mov x2, #17
    mov x16, #4
    svc #0x80

    // 调用 sysctl
    adrp x0, mib@PAGE
    add x0, x0, mib@PAGEOFF
    mov x1, #2
    adrp x2, buffer@PAGE
    add x2, x2, buffer@PAGEOFF
    adrp x3, buffer_size@PAGE
    add x3, x3, buffer_size@PAGEOFF
    mov x4, #0
    mov x5, #0
    mov x16, #202  // sysctl 系统调用号
    svc #0x80

    // 检查返回值
    cmp x0, #0
    b.ne _error

    // 写入成功消息
    mov x0, #1
    adrp x1, success_msg@PAGE
    add x1, x1, success_msg@PAGEOFF
    mov x2, #25
    mov x16, #4
    svc #0x80

    // 写入获取的版本字符串
    mov x0, #1
    adrp x1, buffer@PAGE
    add x1, x1, buffer@PAGEOFF
    mov x2, #256
    mov x16, #4
    svc #0x80

    // 正常退出
    mov x0, #0
    b _exit

_error:
    // 写入错误消息
    mov x0, #1
    adrp x1, error_msg@PAGE
    add x1, x1, error_msg@PAGEOFF
    mov x2, #15
    mov x16, #4
    svc #0x80

    mov x0, #1  // 错误退出码

_exit:
    mov x16, #1
    svc #0x80

.section __TEXT,__cstring
start_msg:
    .asciz "Program started\n"
success_msg:
    .asciz "sysctl call successful:\n"
error_msg:
    .asciz "sysctl failed\n"