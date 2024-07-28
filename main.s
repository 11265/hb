.section __DATA,__data
.align 3
mib:
    .long 6, 3  // 使用 CTL_HW, HW_PHYSMEM
miblen:
    .quad 2
buffer_size:
    .quad 8  // 只需要 8 字节来存储内存大小
result:
    .quad 0

.section __TEXT,__text
.align 2
.globl _main

.extern _sysctl
.extern _malloc
.extern _free
.extern _printf
.extern _errno

_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    // 打印开始信息
    adrp x0, start_msg@PAGE
    add x0, x0, start_msg@PAGEOFF
    bl _printf

    // 设置 sysctl 参数
    sub sp, sp, #48
    adrp x0, mib@PAGE
    add x0, x0, mib@PAGEOFF
    str x0, [sp]
    adrp x1, miblen@PAGE
    ldr x1, [x1, miblen@PAGEOFF]
    str x1, [sp, #8]
    adrp x2, result@PAGE
    add x2, x2, result@PAGEOFF
    str x2, [sp, #16]  // result buffer
    adrp x3, buffer_size@PAGE
    add x3, x3, buffer_size@PAGEOFF
    str x3, [sp, #24]  // size
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

    // 打印 sysctl 成功和结果
    adrp x0, sysctl_success@PAGE
    add x0, x0, sysctl_success@PAGEOFF
    adrp x1, result@PAGE
    ldr x1, [x1, result@PAGEOFF]
    bl _printf

    mov w0, #0
    b .exit

.sysctl_failed:
    bl _errno
    mov w1, w0
    adrp x0, sysctl_failed_msg@PAGE
    add x0, x0, sysctl_failed_msg@PAGEOFF
    bl _printf
    mov w0, #-1

.exit:
    add sp, sp, #48
    ldp x29, x30, [sp], #16
    ret

.section __TEXT,__cstring
start_msg:
    .asciz "Program started\n"
sysctl_success:
    .asciz "sysctl call successful. Physical memory: %lld bytes\n"
sysctl_failed_msg:
    .asciz "sysctl call failed with errno: %d\n"
sysctl_params:
    .asciz "sysctl params: mib=%p, miblen=%lld, buffer=%p, size=%p\n"