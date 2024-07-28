.section __TEXT,__text
.globl _get_pid_by_name
.p2align 2

_get_pid_by_name:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #32
    str x0, [sp, #24]  // 保存进程名称指针

    // 打印调试信息
    adrp x0, debug_start@PAGE
    add x0, x0, debug_start@PAGEOFF
    bl _printf

    // 分配固定大小的缓冲区（64KB）
    mov x0, #65536
    str x0, [sp, #16]  // 保存缓冲区大小
    bl _malloc
    str x0, [sp, #8]   // 保存分配的内存指针

    // 检查内存分配是否成功
    cbz x0, _allocation_failed

    // 打印调试信息
    adrp x0, debug_malloc@PAGE
    add x0, x0, debug_malloc@PAGEOFF
    ldr x1, [sp, #8]
    ldr x2, [sp, #16]
    bl _printf

    // 调用 proc_listpids
    mov w0, #1  // PROC_ALL_PIDS
    mov x1, #0
    ldr x2, [sp, #8]
    ldr x3, [sp, #16]
    bl _proc_listpids

    // 检查返回值
    str w0, [sp, #4]  // 保存返回值
    cmp w0, #0
    ble _proc_listpids_done

    // 打印调试信息
    adrp x0, debug_listpids@PAGE
    add x0, x0, debug_listpids@PAGEOFF
    ldr w1, [sp, #4]
    bl _printf

    // 处理返回的 PID 列表
    ldr x0, [sp, #8]   // 缓冲区指针
    ldr w1, [sp, #4]   // 返回的字节数
    ldr x2, [sp, #24]  // 目标进程名称
    bl _find_pid_in_list

    str w0, [sp]  // 保存找到的 PID 或错误码

    b _cleanup

_proc_listpids_done:
    mov w0, #0
    str w0, [sp]  // 未找到进程

_cleanup:
    ldr x0, [sp, #8]
    bl _free

    // 打印调试信息
    adrp x0, debug_end@PAGE
    add x0, x0, debug_end@PAGEOFF
    bl _printf

    ldr w0, [sp]  // 加载返回值
    add sp, sp, #32
    ldp x29, x30, [sp], #16
    ret

_allocation_failed:
    mov w0, #-1  // 返回错误码
    str w0, [sp]

    // 打印调试信息
    adrp x0, debug_alloc_failed@PAGE
    add x0, x0, debug_alloc_failed@PAGEOFF
    bl _printf

    b _cleanup

_find_pid_in_list:
    // x0: 缓冲区指针
    // w1: 字节数
    // x2: 目标进程名称
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #32
    str x0, [sp, #24]
    str w1, [sp, #20]
    str x2, [sp, #8]

    mov w3, #0  // 索引

_find_loop:
    ldr w4, [x0, x3, lsl #2]  // 加载 PID
    str w4, [sp, #16]  // 保存当前 PID

    // 获取进程名称
    sub sp, sp, #1024
    mov x0, sp
    mov w1, #1024
    ldr w2, [sp, #1040]  // 加载当前 PID
    bl _proc_name

    // 比较进程名称
    ldr x0, [sp, #1032]  // 加载目标进程名称
    mov x1, sp
    bl _strcmp

    add sp, sp, #1024

    cbz w0, _found_pid  // 如果找到匹配的进程名称

    add w3, w3, #1
    ldr w0, [sp, #20]  // 加载字节数
    lsr w0, w0, #2     // 除以 4 得到 PID 数量
    cmp w3, w0
    blo _find_loop

    mov w0, #0  // 未找到进程
    b _find_exit

_found_pid:
    ldr w0, [sp, #16]  // 返回找到的 PID

    // 打印调试信息
    adrp x1, debug_found@PAGE
    add x1, x1, debug_found@PAGEOFF
    mov w2, w0
    mov x0, x1
    bl _printf

_find_exit:
    add sp, sp, #32
    ldp x29, x30, [sp], #16
    ret

.section __DATA,__data
debug_start:
    .asciz "Debug: Starting get_pid_by_name\n"
debug_malloc:
    .asciz "Debug: Malloc returned %p (size: %d)\n"
debug_listpids:
    .asciz "Debug: proc_listpids returned %d bytes\n"
debug_found:
    .asciz "Debug: Found PID %d\n"
debug_end:
    .asciz "Debug: Ending get_pid_by_name\n"
debug_alloc_failed:
    .asciz "Debug: Memory allocation failed\n"