.section __TEXT,__text
.globl _get_pid_by_name
.p2align 2

_get_pid_by_name:
    stp x19, x20, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    mov x19, x0  // 保存进程名称指针

    // 打印调试信息
    adrp x0, debug_start@PAGE
    add x0, x0, debug_start@PAGEOFF
    bl _printf

    // 分配固定大小的缓冲区（64KB）
    mov x23, #65536  // 64KB
    mov x0, x23
    bl _malloc
    mov x20, x0  // 保存分配的内存指针

    // 检查内存分配是否成功
    cbz x20, _allocation_failed

    // 打印调试信息
    adrp x0, debug_malloc@PAGE
    add x0, x0, debug_malloc@PAGEOFF
    mov x1, x20
    bl _printf

    mov x25, #0  // 偏移量
    mov x26, #0  // 总字节数

_proc_listpids_loop:
    // 打印调试信息
    adrp x0, debug_before_listpids@PAGE
    add x0, x0, debug_before_listpids@PAGEOFF
    mov x1, x25
    mov x2, x26
    mov x3, x23
    bl _printf

    // 调用 proc_listpids
    mov x0, #1  // PROC_ALL_PIDS
    mov x1, x25 // 起始偏移量
    add x2, x20, x26  // 缓冲区 + 已处理的字节数
    sub x3, x23, x26  // 剩余缓冲区大小
    bl _proc_listpids

    // 检查返回值
    cmp x0, #0
    ble _proc_listpids_error

    // 打印调试信息
    adrp x0, debug_after_listpids@PAGE
    add x0, x0, debug_after_listpids@PAGEOFF
    mov x1, x0
    bl _printf

    add x26, x26, x0  // 更新总字节数
    
    // 打印调试信息
    adrp x0, debug_listpids@PAGE
    add x0, x0, debug_listpids@PAGEOFF
    mov x1, x26
    bl _printf

    // 检查是否超出缓冲区
    cmp x26, x23
    bge _buffer_overflow

    // 处理此批次的 PID
    mov x21, #0  // PID 索引
_pid_loop:
    ldr w0, [x20, x21, lsl #2]  // 加载 PID
    
    // 获取进程名称
    sub sp, sp, #1024
    mov x1, sp
    mov x2, #1024
    bl _proc_name

    // 比较进程名称
    mov x0, x19  // 目标进程名称
    mov x1, sp   // 当前进程名称
    bl _strcmp

    add sp, sp, #1024

    cbz w0, _found_pid  // 如果找到匹配的进程名称

    add x21, x21, #1
    lsl x22, x21, #2
    cmp x22, x26
    blt _pid_loop

    // 更新偏移量并继续循环
    add x25, x25, x21
    mov x26, #0
    b _proc_listpids_loop

_proc_listpids_error:
    // 打印调试信息
    adrp x0, debug_listpids_error@PAGE
    add x0, x0, debug_listpids_error@PAGEOFF
    bl _printf
    mov x0, #-2  // 错误代码
    b _cleanup

_buffer_overflow:
    // 打印调试信息
    adrp x0, debug_buffer_overflow@PAGE
    add x0, x0, debug_buffer_overflow@PAGEOFF
    bl _printf
    mov x0, #-3  // 错误代码
    b _cleanup

_found_pid:
    ldr w0, [x20, x21, lsl #2]  // 返回找到的 PID

    // 打印调试信息
    adrp x1, debug_found@PAGE
    add x1, x1, debug_found@PAGEOFF
    bl _printf
    b _cleanup

_allocation_failed:
    mov x0, #-1  // 返回错误码

    // 打印调试信息
    adrp x0, debug_alloc_failed@PAGE
    add x0, x0, debug_alloc_failed@PAGEOFF
    bl _printf
    b _exit

_cleanup:
    mov x1, x20
    bl _free

    // 打印调试信息
    adrp x0, debug_end@PAGE
    add x0, x0, debug_end@PAGEOFF
    bl _printf

_exit:
    ldp x29, x30, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x19, x20, [sp], #16
    ret

.section __DATA,__data
debug_start:
    .asciz "Debug: Starting get_pid_by_name\n"
debug_malloc:
    .asciz "Debug: Malloc returned %p\n"
debug_before_listpids:
    .asciz "Debug: Before proc_listpids: offset=%d, total=%d, buffer_size=%d\n"
debug_after_listpids:
    .asciz "Debug: proc_listpids returned %d bytes\n"
debug_listpids:
    .asciz "Debug: Total proc_listpids bytes: %d\n"
debug_found:
    .asciz "Debug: Found PID %d\n"
debug_end:
    .asciz "Debug: Ending get_pid_by_name\n"
debug_alloc_failed:
    .asciz "Debug: Memory allocation failed\n"
debug_listpids_error:
    .asciz "Debug: Error calling proc_listpids\n"
debug_buffer_overflow:
    .asciz "Debug: Buffer overflow detected\n"