.section __TEXT,__text
.globl _get_pid_by_name
.p2align 2

_get_pid_by_name:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #48
    str x0, [sp, #40]  // 保存进程名称指针

    // 打印调试信息
    adrp x0, debug_start@PAGE
    add x0, x0, debug_start@PAGEOFF
    ldr x1, [sp, #40]
    bl _printf

    // 分配固定大小的缓冲区（64KB）
    mov x0, #65536
    str x0, [sp, #32]  // 保存缓冲区大小
    bl _malloc
    str x0, [sp, #24]   // 保存分配的内存指针

    // 检查内存分配是否成功
    cbz x0, _allocation_failed

    // 打印调试信息
    adrp x0, debug_malloc@PAGE
    add x0, x0, debug_malloc@PAGEOFF
    ldr x1, [sp, #24]
    ldr x2, [sp, #32]
    bl _printf

    // 调用 proc_listpids
    mov w0, #1  // PROC_ALL_PIDS
    mov x1, #0
    ldr x2, [sp, #24]
    ldr w3, [sp, #32]
    bl _proc_listpids

    // 检查返回值
    str w0, [sp, #20]  // 保存返回值
    cmp w0, #0
    ble _proc_listpids_done

    // 计算 PID 数量 (字节数 / sizeof(int))
    ldr w1, [sp, #32]  // 缓冲区大小
    cmp w0, w1
    csel w0, w0, w1, lo  // 如果返回值小于缓冲区大小，使用返回值；否则使用缓冲区大小
    lsr w0, w0, #2  // 除以 4 (sizeof(int))
    str w0, [sp, #20]  // 保存 PID 数量

    // 打印调试信息
    adrp x0, debug_listpids@PAGE
    add x0, x0, debug_listpids@PAGEOFF
    ldr w1, [sp, #20]
    bl _printf

    // 处理返回的 PID 列表
    ldr x0, [sp, #24]   // 缓冲区指针
    ldr w1, [sp, #20]   // PID 数量
    ldr x2, [sp, #40]   // 目标进程名称
    bl _find_pid_by_name

    str w0, [sp, #16]  // 保存找到的 PID 或错误码

    b _cleanup

_proc_listpids_done:
    mov w0, #0
    str w0, [sp, #16]  // 未找到进程

_cleanup:
    ldr x0, [sp, #24]
    bl _free

    // 打印调试信息
    adrp x0, debug_end@PAGE
    add x0, x0, debug_end@PAGEOFF
    ldr w1, [sp, #16]
    bl _printf

    ldr w0, [sp, #16]  // 加载返回值
    add sp, sp, #48
    ldp x29, x30, [sp], #16
    ret

_allocation_failed:
    mov w0, #-1  // 返回错误码
    str w0, [sp, #16]

    // 打印调试信息
    adrp x0, debug_alloc_failed@PAGE
    add x0, x0, debug_alloc_failed@PAGEOFF
    bl _printf

    b _cleanup

_find_pid_by_name:
    // x0: PID 缓冲区指针
    // w1: PID 数量
    // x2: 目标进程名称
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #64
    str x0, [sp, #56]
    str w1, [sp, #52]
    str x2, [sp, #40]

    // 打印开始查找的调试信息
    adrp x0, debug_find_start@PAGE
    add x0, x0, debug_find_start@PAGEOFF
    ldr w1, [sp, #52]
    bl _printf

    mov w3, #0  // 索引
    str w3, [sp, #36]  // 保存索引到栈上

_find_loop:
    // 检查索引是否超出范围
    ldr w0, [sp, #52]  // 加载 PID 数量
    ldr w3, [sp, #36]  // 加载索引
    cmp w3, w0
    bhs _find_exit_not_found

    ldr x0, [sp, #56]
    ldr w4, [x0, x3, lsl #2]  // 加载 PID
    str w4, [sp, #48]  // 保存当前 PID

    // 打印正在检查的 PID 和索引
    adrp x0, debug_checking_pid@PAGE
    add x0, x0, debug_checking_pid@PAGEOFF
    ldr w1, [sp, #36]  // 索引
    ldr w2, [sp, #52]  // PID 总数
    ldr w3, [sp, #48]  // 当前 PID
    bl _printf

    // 检查 PID 是否有效 (大于 0 且小于等于 99999)
    ldr w4, [sp, #48]
    cmp w4, #0
    ble _continue_loop
    mov w5, #0x1869F  // 99999 的十六进制表示
    cmp w4, w5
    bgt _continue_loop

    // 获取进程名称
    sub sp, sp, #1024
    mov x0, sp
    mov w1, #1024
    ldr w2, [sp, #1072]  // 加载当前 PID
    bl _proc_name

    cmp w0, #0
    ble _continue_loop

    // 打印进程名称
    adrp x0, debug_proc_name@PAGE
    add x0, x0, debug_proc_name@PAGEOFF
    mov x1, sp
    bl _printf

    // 比较进程名称
    ldr x0, [sp, #1064]  // 加载目标进程名称
    mov x1, sp
    bl _strcmp

    cbz w0, _found_pid  // 如果找到匹配的进程名称

_continue_loop:
    add sp, sp, #1024
    ldr w3, [sp, #36]  // 加载索引
    add w3, w3, #1
    str w3, [sp, #36]  // 保存更新后的索引
    b _find_loop

_found_pid:
    add sp, sp, #1024
    ldr w0, [sp, #48]  // 返回找到的 PID

    // 打印调试信息
    adrp x1, debug_found@PAGE
    add x1, x1, debug_found@PAGEOFF
    mov w2, w0
    mov x0, x1
    bl _printf
    b _find_exit

_find_exit_not_found:
    mov w0, #0  // 未找到进程

_find_exit:
    // 打印结束查找的调试信息
    adrp x1, debug_find_end@PAGE
    add x1, x1, debug_find_end@PAGEOFF
    mov w2, w0  // 返回值
    mov x0, x1
    bl _printf

    add sp, sp, #64
    ldp x29, x30, [sp], #16
    ret

.section __DATA,__data
debug_start:
    .asciz "Debug: Starting get_pid_by_name for process '%s'\n"
debug_malloc:
    .asciz "Debug: Malloc returned %p (size: %d)\n"
debug_listpids:
    .asciz "Debug: proc_listpids returned %d PIDs\n"
debug_found:
    .asciz "Debug: Found PID %d\n"
debug_end:
    .asciz "Debug: Ending get_pid_by_name, result: %d\n"
debug_alloc_failed:
    .asciz "Debug: Memory allocation failed\n"
debug_checking_pid:
    .asciz "Debug: Checking index %d/%d, PID %d\n"
debug_proc_name:
    .asciz "Debug: Process name: %s\n"
debug_find_start:
    .asciz "Debug: Starting to find PID in list of %d PIDs\n"
debug_find_end:
    .asciz "Debug: Finished finding PID, result: %d\n"