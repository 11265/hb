.section __DATA,__data
.align 3
buffer_size:
    .quad 8  // 8 bytes for uint64_t
result:
    .quad 0

.section __TEXT,__text
.align 2
.globl _main

.extern _sysctlbyname
.extern _printf
.extern _errno
.extern _exit

_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    // 打印开始信息
    adrp x0, start_msg@PAGE
    add x0, x0, start_msg@PAGEOFF
    bl _printf

    // 准备 sysctlbyname 参数
    adrp x0, sysctl_name@PAGE
    add x0, x0, sysctl_name@PAGEOFF
    adrp x1, result@PAGE
    add x1, x1, result@PAGEOFF
    adrp x2, buffer_size@PAGE
    add x2, x2, buffer_size@PAGEOFF
    ldr x2, [x2]
    mov x3, xzr
    mov x4, xzr

    // 打印参数
    sub sp, sp, #32
    stp x0, x1, [sp]
    stp x2, x3, [sp, #16]
    adrp x0, sysctl_params@PAGE
    add x0, x0, sysctl_params@PAGEOFF
    ldr x1, [sp]
    ldr x2, [sp, #8]
    ldr x3, [sp, #16]
    bl _printf
    add sp, sp, #32

    // 打印准备调用 sysctlbyname
    adrp x0, calling_sysctlbyname@PAGE
    add x0, x0, calling_sysctlbyname@PAGEOFF
    bl _printf

    // 调用 sysctlbyname
    adrp x0, sysctl_name@PAGE
    add x0, x0, sysctl_name@PAGEOFF
    adrp x1, result@PAGE
    add x1, x1, result@PAGEOFF
    adrp x2, buffer_size@PAGE
    add x2, x2, buffer_size@PAGEOFF
    ldr x2, [x2]
    mov x3, xzr
    mov x4, xzr
    bl _sysctlbyname

    // 打印 sysctlbyname 返回
    adrp x0, sysctlbyname_returned@PAGE
    add x0, x0, sysctlbyname_returned@PAGEOFF
    bl _printf

    // 检查返回值
    cmp w0, #0
    bne .sysctl_failed

    // 打印成功和结果
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
    // 打印退出信息
    adrp x0, exit_msg@PAGE
    add x0, x0, exit_msg@PAGEOFF
    bl _printf

    // 调用 exit 系统调用
    mov x0, #0
    mov x16, #1  // exit 系统调用号
    svc #0x80

    // 这里不应该被执行到
    ldp x29, x30, [sp], #16
    ret

.section __TEXT,__cstring
start_msg:
    .asciz "Program started\n"
sysctl_name:
    .asciz "hw.memsize"
sysctl_success:
    .asciz "sysctlbyname call successful. Physical memory: %lld bytes\n"
sysctl_failed_msg:
    .asciz "sysctlbyname call failed with errno: %d\n"
sysctl_params:
    .asciz "sysctlbyname params: name=%p, buffer=%p, size=%lld\n"
calling_sysctlbyname:
    .asciz "Calling sysctlbyname...\n"
sysctlbyname_returned:
    .asciz "sysctlbyname returned\n"
exit_msg:
    .asciz "Program exiting\n"