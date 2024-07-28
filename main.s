.section __DATA,__data
.align 3
KERN_SUCCESS:
    .long 0
    .long 0
mib:
    .long 1, 14, 0, 0
miblen:
    .long 4
    .long 0
buffer_size:
    .quad 1048576  // 增加到 1MB
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

_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    // 分配内存
    adrp x0, buffer_size@PAGE
    ldr x0, [x0, buffer_size@PAGEOFF]
    bl _malloc
    mov x19, x0  // x19 = buffer

    // 设置 sysctl 参数
    sub sp, sp, #48
    adrp x0, mib@PAGE
    add x0, x0, mib@PAGEOFF
    str x0, [sp]
    adrp x1, miblen@PAGE
    ldr w1, [x1, miblen@PAGEOFF]
    str w1, [sp, #8]
    str x19, [sp, #16]  // buffer
    adrp x2, buffer_size@PAGE
    add x2, x2, buffer_size@PAGEOFF
    str x2, [sp, #24]  // &size
    stp xzr, xzr, [sp, #32]  // 新的参数：NULL, 0

    // 调用 sysctl
    mov x0, sp
    bl _sysctl

    // 检查返回值
    cmp w0, #0
    bne .cleanup

    // 遍历进程列表
    mov x20, x19  // x20 = current process
    adrp x21, buffer_size@PAGE
    ldr x21, [x21, buffer_size@PAGEOFF]  // x21 = buffer size
    adrp x22, process_name@PAGE
    add x22, x22, process_name@PAGEOFF  // x22 = target process name

.loop:
    ldr x0, [x20, #52]  // 进程名通常在 kp_proc.p_comm
    mov x1, x22
    bl _strcmp
    cbz w0, .found_process

    ldr w23, [x20, #16]  // 获取 kp_proc.p_len
    add x20, x20, x23  // 移动到下一个进程
    sub x21, x21, x23
    cmp x21, #0
    bgt .loop

    b .not_found

.found_process:
    // 进程找到，PID 在 kp_proc.p_pid
    ldr w0, [x20, #24]
    // 打印 PID
    adrp x1, pid_format@PAGE
    add x1, x1, pid_format@PAGEOFF
    bl _printf
    b .cleanup

.not_found:
    adrp x0, not_found_msg@PAGE
    add x0, x0, not_found_msg@PAGEOFF
    bl _printf
    mov w0, #-1

.cleanup:
    // 释放内存
    mov x0, x19
    bl _free

    // 恢复寄存器并返回
    ldp x29, x30, [sp], #16
    ret

.section __TEXT,__cstring
pid_format:
    .asciz "PID: %d\n"
not_found_msg:
    .asciz "Process not found\n"