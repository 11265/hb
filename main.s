.section __DATA,__data
.align 3
mib:
    .long 1, 14, 0, 0, 0  // CTL_KERN, KERN_PROC, KERN_PROC_ALL
buffer:
    .space 1024*1024  // 1MB buffer for process info
buffer_size:
    .quad 1024*1024
process_name:
    .asciz "pvz"  // 要查找的进程名称，可以根据需要修改

.section __TEXT,__text
.globl _main
.align 2

_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    // 调用 sysctl 获取进程列表
    adrp x0, mib@PAGE
    add x0, x0, mib@PAGEOFF
    mov x1, #5  // mib 数组的长度
    adrp x2, buffer@PAGE
    add x2, x2, buffer@PAGEOFF
    adrp x3, buffer_size@PAGE
    add x3, x3, buffer_size@PAGEOFF
    mov x4, #0
    mov x5, #0
    mov x16, #202  // sysctl 系统调用号
    svc #0x80

    // 保存返回值
    mov x19, x0

    // 打印返回值
    adrp x0, sysctl_return_msg@PAGE
    add x0, x0, sysctl_return_msg@PAGEOFF
    mov x1, x19
    bl _printf

    // 检查返回值
    cmp x19, #0
    b.eq _success

    // 如果失败，获取 errno
    bl ___error
    ldr w1, [x0]

    // 打印 errno
    adrp x0, error_msg@PAGE
    add x0, x0, error_msg@PAGEOFF
    bl _printf

    b _exit

_success:
    // 打印成功消息
    adrp x0, success_msg@PAGE
    add x0, x0, success_msg@PAGEOFF
    bl _printf

_exit:
    mov x0, #0
    mov x16, #1  // exit 系统调用
    svc #0x80

.section __TEXT,__cstring
sysctl_return_msg:
    .asciz "sysctl return value: %d\n"
error_msg:
    .asciz "sysctl failed with errno: %d\n"
success_msg:
    .asciz "sysctl call successful\n"