.section __TEXT,__text
.globl _main
.p2align 2

_main:
    // ... (前面的代码保持不变)

    // 通过名称获取指定进程 ID
    adrp x0, process_name@PAGE
    add x0, x0, process_name@PAGEOFF
    bl _get_pid_by_name

    // 检查返回值
    cmp x0, #0
    beq _not_found
    cmp x0, #-1
    beq _allocation_error
    cmp x0, #-2
    beq _buffer_overflow_error

    // 保存找到的 PID
    mov x19, x0

    // 打印找到的进程 ID
    adrp x0, found_pid_format@PAGE
    add x0, x0, found_pid_format@PAGEOFF
    mov x1, x19
    bl _printf

    b _exit_program

_not_found:
    // 打印未找到进程的消息
    adrp x0, not_found_message@PAGE
    add x0, x0, not_found_message@PAGEOFF
    bl _printf
    b _exit_program

_allocation_error:
    // 打印内存分配错误消息
    adrp x0, allocation_error_message@PAGE
    add x0, x0, allocation_error_message@PAGEOFF
    bl _printf
    b _exit_program

_buffer_overflow_error:
    // 打印缓冲区溢出错误消息
    adrp x0, buffer_overflow_message@PAGE
    add x0, x0, buffer_overflow_message@PAGEOFF
    bl _printf

_exit_program:
    // 恢复寄存器并返回
    ldp x29, x30, [sp], #16
    ldp x19, x20, [sp], #16
    mov w0, #0
    ret

.section __DATA,__data
message:
    .asciz "iOS Assembly!\n"
current_pid_format:
    .asciz "Current PID: %d\n"
found_pid_format:
    .asciz "Found process PID: %d\n"
not_found_message:
    .asciz "Process not found\n"
allocation_error_message:
    .asciz "Memory allocation error\n"
buffer_overflow_message:
    .asciz "Buffer overflow error\n"
process_name:
    .asciz "SpringBoard"