.section __DATA,__data
.align 3
mib:
    .long 1, 14, 0, 0
miblen:
    .quad 4
buffer_size:
    .quad 1048576  // 1MB
process_name:
    .asciz "pvz"
    .align 3

.section __TEXT,__text
.align 2
.globl _main

.extern _sysctl
.extern _malloc
.extern _free
.extern _strcmp
.extern _printf
.extern _errno

_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    // 打印开始信息
    adrp x0, start_msg@PAGE
    add x0, x0, start_msg@PAGEOFF
    bl _printf

    // 分配内存
    adrp x0, buffer_size@PAGE
    ldr x0, [x0, buffer_size@PAGEOFF]
    bl _malloc
    mov x19, x0  // x19 = buffer

    // 检查内存分配
    cbz x19, .malloc_failed

    // 打印内存分配成功
    adrp x0, malloc_success@PAGE
    add x0, x0, malloc_success@PAGEOFF
    bl _printf

    // 设置 sysctl 参数
    sub sp, sp, #48
    adrp x0, mib@PAGE
    add x0, x0, mib@PAGEOFF
    str x0, [sp]
    adrp x1, miblen@PAGE
    ldr x1, [x1, miblen@PAGEOFF]
    str x1, [sp, #8]
    str x19, [sp, #16]  // buffer
    adrp x2, buffer_size@PAGE
    ldr x2, [x2, buffer_size@PAGEOFF]
    str x2, [sp, #24]  // size
    stp xzr, xzr, [sp, #32]  // 新的参数：NULL, 0

    // 打印 sysctl 参数
    adrp x0, sysctl_params@PAGE
    add x0, x0, sysctl_params@PAGEOFF
    ldr x1, [sp]
    ldr x2, [sp, #8]
    ldr x3, [sp, #16]
    ldr x4, [sp, #24]
    bl _printf

    // 调用 sysctl
    mov x0, sp
    bl _sysctl

    // 检查返回值
    cmp w0, #0
    bne .sysctl_failed

    // 打印 sysctl 成功
    adrp x0, sysctl_success@PAGE
    add x0, x0, sysctl_success@PAGEOFF
    bl _printf

    // 清理并退出
    mov x0, x19
    bl _free

    mov w0, #0
    ldp x29, x30, [sp], #16
    ret

.malloc_failed:
    adrp x0, malloc_failed_msg@PAGE
    add x0, x0, malloc_failed_msg@PAGEOFF
    bl _printf
    mov w0, #-1
    ldp x29, x30, [sp], #16
    ret

.sysctl_failed:
    bl _errno
    mov w1, w0
    adrp x0, sysctl_failed_msg@PAGE
    add x0, x0, sysctl_failed_msg@PAGEOFF
    bl _printf
    mov x0, x19
    bl _free
    mov w0, #-1
    ldp x29, x30, [sp], #16
    ret

.section __TEXT,__cstring
start_msg:
    .asciz "Program started\n"
malloc_success:
    .asciz "Memory allocated successfully\n"
sysctl_success:
    .asciz "sysctl call successful\n"
malloc_failed_msg:
    .asciz "Memory allocation failed\n"
sysctl_failed_msg:
    .asciz "sysctl call failed with errno: %d\n"
sysctl_params:
    .asciz "sysctl params: mib=%p, miblen=%lld, buffer=%p, size=%lld\n"