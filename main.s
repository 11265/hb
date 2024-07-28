.section __DATA,__data
.align 3
mib:
    .long 6, 3  // CTL_HW, HW_PHYSMEM
miblen:
    .quad 2
buffer_size:
    .quad 8  // 8 bytes for uint64_t
result:
    .quad 0

.section __TEXT,__text
.align 2
.globl _main

.extern _sysctl
.extern _printf
.extern _errno

_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    // 打印开始信息
    adrp x0, start_msg@PAGE
    add x0, x0, start_msg@PAGEOFF
    bl _printf

    // 准备 sysctl 参数
    sub sp, sp, #64
    stp xzr, xzr, [sp]
    stp xzr, xzr, [sp, #16]
    stp xzr, xzr, [sp, #32]
    stp xzr, xzr, [sp, #48]

    // 设置 mib
    adrp x0, mib@PAGE
    add x0, x0, mib@PAGEOFF
    str x0, [sp]

    // 设置 miblen
    adrp x0, miblen@PAGE
    ldr x0, [x0, miblen@PAGEOFF]
    str x0, [sp, #8]

    // 设置 result 缓冲区地址
    adrp x0, result@PAGE
    add x0, x0, result@PAGEOFF
    str x0, [sp, #16]

    // 设置 buffer_size
    adrp x0, buffer_size@PAGE
    ldr x0, [x0, buffer_size@PAGEOFF]
    str x0, [sp, #24]

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
    add sp, sp, #64
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