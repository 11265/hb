.section __TEXT,__text
.globl _main
.p2align 2

_main:
    // 保存调用者保存的寄存器
    stp x19, x20, [sp, #-16]!
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    // 打印欢迎消息
    adrp x0, message@PAGE
    add x0, x0, message@PAGEOFF
    bl _printf

    // 获取当前进程 ID
    bl _getpid
    mov x19, x0  // 保存当前进程 ID

    // 打印当前进程 ID
    adrp x0, current_pid_format@PAGE
    add x0, x0, current_pid_format@PAGEOFF
    mov x1, x19
    bl _printf

_exit_program:
    // 恢复寄存器并返回
    ldp x29, x30, [sp], #16
    ldp x19, x20, [sp], #16
    mov w0, #0
    ret

.section __DATA,__data
message:
    .asciz "iOS Assembly: Get Current PID\n"
current_pid_format:
    .asciz "Current PID: %d\n"
