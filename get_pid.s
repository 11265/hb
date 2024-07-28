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

    mov x19, x0  // Save process name pointer

    adrp x0, log_entry@PAGE
    add x0, x0, log_entry@PAGEOFF
    bl _printf

    mov x0, #16384  // 16KB for PID list
    bl _malloc
    cmp x0, #0
    beq _malloc_failed
    mov x20, x0  // Save allocated memory pointer

    adrp x0, log_after_malloc@PAGE
    add x0, x0, log_after_malloc@PAGEOFF
    mov x1, x20
    bl _printf

    mov x0, #1  // PROC_ALL_PIDS
    mov x1, #0
    mov x2, x20  // buffer
    mov x3, #16384
    bl _proc_listpids

    cmp x0, #0
    ble _proc_listpids_failed
    cmp x0, #16384
    bgt _buffer_overflow

    mov x21, x0  // Save returned byte count
    adrp x0, log_after_listpids@PAGE
    add x0, x0, log_after_listpids@PAGEOFF
    mov x1, x21
    bl _printf

    mov x22, #4  // Size of pid_t
    udiv x23, x21, x22  // Calculate number of PIDs
    adrp x0, log_pid_count@PAGE
    add x0, x0, log_pid_count@PAGEOFF
    mov x1, x23
    bl _printf

    mov x0, #256  // Allocate 256 bytes for process name
    bl _malloc
    cmp x0, #0
    beq _malloc_failed
    mov x24, x0  // Save process name buffer

    mov x25, #0  // PID counter

_pid_loop:
    cmp x25, x23
    bge _not_found

    adrp x0, log_pid_loop@PAGE
    add x0, x0, log_pid_loop@PAGEOFF
    mov x1, x25
    bl _printf

    ldr w26, [x20, x25, lsl #2]  // Load PID
    
    mov x0, x26  // PID
    mov x1, x24  // Buffer
    mov x2, #256  // Buffer size
    bl _proc_name

    cmp x0, #0
    ble _next_pid  // Skip if proc_name fails

    adrp x0, log_proc_name@PAGE
    add x0, x0, log_proc_name@PAGEOFF
    mov x1, x24
    bl _printf

    mov x0, x19  // Target process name
    mov x1, x24  // Current process name
    bl _strcmp

    cmp w0, #0
    beq _found_pid  // If matching process name found

_next_pid:
    add x25, x25, #1
    b _pid_loop

_not_found:
    mov x26, #-1  // Process not found
    b _cleanup

_found_pid:
    // x26 already contains the found PID

_cleanup:
    adrp x0, log_before_free@PAGE
    add x0, x0, log_before_free@PAGEOFF
    mov x1, x26
    bl _printf

    mov x0, x24
    bl _free
    mov x0, x20
    bl _free

    adrp x0, log_exit@PAGE
    add x0, x0, log_exit@PAGEOFF
    bl _printf

    mov x0, x26  // Return PID or -1
    ldp x29, x30, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x19, x20, [sp], #16
    ret

_malloc_failed:
    adrp x0, log_malloc_failed@PAGE
    add x0, x0, log_malloc_failed@PAGEOFF
    bl _printf
    mov x26, #-1
    b _cleanup

_proc_listpids_failed:
    adrp x0, log_proc_listpids_failed@PAGE
    add x0, x0, log_proc_listpids_failed@PAGEOFF
    bl _printf
    mov x26, #-1
    b _cleanup

_buffer_overflow:
    adrp x0, log_buffer_overflow@PAGE
    add x0, x0, log_buffer_overflow@PAGEOFF
    bl _printf
    mov x26, #-1
    b _cleanup

.section __DATA,__data
log_entry:
    .asciz "Entered get_pid_by_name\n"
log_after_malloc:
    .asciz "After malloc, buffer at: %p\n"
log_after_listpids:
    .asciz "After proc_listpids, returned: %d bytes\n"
log_pid_count:
    .asciz "Total PIDs: %d\n"
log_pid_loop:
    .asciz "PID loop iteration: %d\n"
log_proc_name:
    .asciz "Process name: %s\n"
log_before_free:
    .asciz "Before free, PID: %d\n"
log_exit:
    .asciz "Exiting get_pid_by_name\n"
log_malloc_failed:
    .asciz "Malloc failed\n"
log_proc_listpids_failed:
    .asciz "proc_listpids failed\n"
log_buffer_overflow:
    .asciz "Buffer overflow in proc_listpids\n"