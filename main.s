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

    // 检查返回值
    cmp x0, #0
    b.ne _error

    // 遍历进程列表
    adrp x19, buffer@PAGE
    add x19, x19, buffer@PAGEOFF
    adrp x20, buffer_size@PAGE
    add x20, x20, buffer_size@PAGEOFF
    ldr x20, [x20]
    add x20, x19, x20  // 结束地址

_loop:
    cmp x19, x20
    b.ge _not_found

    // 获取进程名称
    add x0, x19, #0x1bc  // proc_name 在 kinfo_proc 结构中的偏移
    adrp x1, process_name@PAGE
    add x1, x1, process_name@PAGEOFF
    bl _strcmp

    // 如果名称匹配，提取 PID
    cmp x0, #0
    b.eq _found

    // 移动到下一个 kinfo_proc
    add x19, x19, #0x260  // kinfo_proc 结构的大小
    b _loop

_found:
    // 提取 PID
    ldr w1, [x19, #0x68]  // PID 在 kinfo_proc 结构中的偏移

    // 打印 PID
    adrp x0, found_msg@PAGE
    add x0, x0, found_msg@PAGEOFF
    bl _printf
    b _exit

_not_found:
    // 打印未找到消息
    adrp x0, not_found_msg@PAGE
    add x0, x0, not_found_msg@PAGEOFF
    bl _printf
    b _exit

_error:
    // 打印错误消息
    adrp x0, error_msg@PAGE
    add x0, x0, error_msg@PAGEOFF
    bl _printf

_exit:
    mov x0, #0
    mov x16, #1  // exit 系统调用
    svc #0x80

.section __TEXT,__cstring
found_msg:
    .asciz "Process PID: %d\n"
not_found_msg:
    .asciz "Process not found\n"
error_msg:
    .asciz "sysctl failed\n"

.section __TEXT,__text
.globl _strcmp
_strcmp:
    ldrb w2, [x0], #1
    ldrb w3, [x1], #1
    cmp w2, w3
    b.ne _strcmp_ne
    cmp w2, #0
    b.ne _strcmp
    mov x0, #0
    ret
_strcmp_ne:
    sub x0, x2, x3
    ret