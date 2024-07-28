.section __TEXT,__text
.globl _get_pid_by_name
.p2align 2

_get_pid_by_name:
    stp x19, x20, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    mov x19, x0  // 保存进程名称指针

    // 打印调试信息
    adrp x0, debug_start@PAGE
    add x0, x0, debug_start@PAGEOFF
    bl _printf

    // 首先调用 proc_listpids 获取所需的缓冲区大小
    mov x0, #1  // PROC_ALL_PIDS
    mov x1, #0
    mov x2, #0  // NULL buffer
    mov x3, #0  // 0 size
    bl _proc_listpids

    // 检查返回值
    cmp x0, #0
    ble _proc_listpids_error

    // 保存所需的缓冲区大小
    mov x23, x0
    add x23, x23, #4096  // 添加一些额外的空间以防万一

    // 打印调试信息
    adrp x0, debug_buffer_size@PAGE
    add x0, x0, debug_buffer_size@PAGEOFF
    mov x1, x23
    bl _printf

    // 分配内存
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

    // 再次调用 proc_listpids，这次使用分配的缓冲区
    mov x0, #1  // PROC_ALL_PIDS
    mov x1, #0
    mov x2, x20  // buffer
    mov x3, x23  // size
    bl _proc_listpids

    // 检查返回值
    cmp x0, #0
    ble _cleanup

    mov x21, x0  // 保存返回的字节数
    mov x22, #0  // PID 计数器

    // 打印调试信息
    adrp x0, debug_listpids@PAGE
    add x0, x0, debug_listpids@PAGEOFF
    mov x1, x21
    bl _printf

_pid_loop:
    ldr w0, [x20, x22, lsl #2]  // 加载 PID
    
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

    add x22, x22, #1
    lsl x0, x22, #2
    cmp x0, x21
    blt _pid_loop

    mov x0, #0  // 未找到进程
    b _cleanup

_found_pid:
    ldr w0, [x20, x22, lsl #2]  // 返回找到的 PID

    // 打印调试信息
    adrp x1, debug_found@PAGE
    add x1, x1, debug_found@PAGEOFF
    bl _printf

_cleanup:
    mov x1, x20
    bl _free

    // 打印调试信息
    adrp x0, debug_end@PAGE
    add x0, x0, debug_end@PAGEOFF
    bl _printf

    b _exit

_allocation_failed:
    mov x0, #-1  // 返回错误码

    // 打印调试信息
    adrp x0, debug_alloc_failed@PAGE
    add x0, x0, debug_alloc_failed@PAGEOFF
    bl _printf
    b _exit

_proc_listpids_error:
    mov x0, #-2  // 返回错误码

    // 打印调试信息
    adrp x0, debug_proc_listpids_error@PAGE
    add x0, x0, debug_proc_listpids_error@PAGEOFF
    bl _printf

_exit:
    ldp x29, x30, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x19, x20, [sp], #16
    ret

.section __DATA,__data
debug_start:
    .asciz "Debug: Starting get_pid_by_name\n"
debug_buffer_size:
    .asciz "Debug: Required buffer size: %d bytes\n"
debug_malloc:
    .asciz "Debug: Malloc returned %p\n"
debug_listpids:
    .asciz "Debug: proc_listpids returned %d bytes\n"
debug_found:
    .asciz "Debug: Found PID %d\n"
debug_end:
    .asciz "Debug: Ending get_pid_by_name\n"
debug_alloc_failed:
    .asciz "Debug: Memory allocation failed\n"
debug_proc_listpids_error:
    .asciz "Debug: Error calling proc_listpids\n"