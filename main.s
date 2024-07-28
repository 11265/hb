.section __TEXT,__text
.globl _main
.p2align 2

_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    adrp x0, msg@PAGE
    add x0, x0, msg@PAGEOFF
    bl _printf

    bl _getpid
    mov x19, x0

    adrp x0, pid_msg@PAGE
    add x0, x0, pid_msg@PAGEOFF
    mov x1, x19
    bl _printf

    adrp x0, before_call@PAGE
    add x0, x0, before_call@PAGEOFF
    bl _printf

    adrp x0, target_process@PAGE
    add x0, x0, target_process@PAGEOFF
    bl _get_pid_by_name

    mov x20, x0

    adrp x0, result_msg@PAGE
    add x0, x0, result_msg@PAGEOFF
    mov x1, x20
    bl _printf

    mov x0, #0
    ldp x29, x30, [sp], #16
    ret

.section __DATA,__data
msg:
    .asciz "iOS Assembly!\n"
pid_msg:
    .asciz "Current PID: %d\n"
before_call:
    .asciz "Before calling get_pid_by_name\n"
result_msg:
    .asciz "Found PID: %d\n"
target_process:
    .asciz "pvz"  // 替换为您想要查找的进程名