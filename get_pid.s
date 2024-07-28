.section __TEXT,__text
.globl _get_pid_by_name
.p2align 2

_get_pid_by_name:
    stp x19, x20, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    mov x19, x0  // Save process name pointer

    adrp x0, log_entry@PAGE
    add x0, x0, log_entry@PAGEOFF
    bl _printf

    mov x0, #16384  // 16KB for PID list
    bl _malloc
    cmp x0, #0
    beq _cleanup   // Jump to cleanup if malloc fails
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
    ble _cleanup   // Jump to cleanup if proc_listpids fails or returns 0
    cmp x0, #16384
    bgt _cleanup   // Jump to cleanup if proc_listpids returns more than our buffer size

    mov x21, x0  // Save returned byte count
    adrp x0, log_after_listpids@PAGE
    add x0, x0, log_after_listpids@PAGEOFF
    mov x1, x21
    bl _printf

    mov x22, #0  // PID counter
    udiv x23, x21, #4  // Calculate number of PIDs (byte count / 4)
    adrp x0, log_pid_count@PAGE
    add x0, x0, log_pid_count@PAGEOFF
    mov x1, x23
    bl _printf

    mov x0, #256  // Allocate 256 bytes for process name
    bl _malloc
    cmp x0, #0
    beq _cleanup   // Jump to cleanup if malloc fails
    mov x24, x0  // Save process name buffer

_pid_loop:
    cmp x22, x23
    bge _not_found

    adrp x0, log_pid_loop@PAGE
    add x0, x0, log_pid_loop@PAGEOFF
    mov x1, x22
    bl _printf

    ldr w0, [x20, x22, lsl #2]  // Load PID
    
    mov x1, x24
    mov x2, #256
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
    add x22, x22, #1
    b _pid_loop

_not_found:
    mov x0, #-1  // Process not found
    b _cleanup

_found_pid:
    ldr w0, [x20, x22, lsl #2]  // Return found PID

_cleanup:
    adrp x1, log_before_free@PAGE
    add x1, x1, log_before_free@PAGEOFF
    mov x2, x0
    bl _printf

    cmp x24, #0    // Check if process name buffer is NULL
    beq _free_pid_buffer
    mov x0, x24
    bl _free
    mov x24, #0    // Set to NULL after freeing

_free_pid_buffer:
    cmp x20, #0    // Check if PID buffer pointer is NULL
    beq _exit      // If NULL, skip freeing
    mov x0, x20
    bl _free
    mov x20, #0    // Set to NULL after freeing

_exit:
    adrp x0, log_exit@PAGE
    add x0, x0, log_exit@PAGEOFF
    bl _printf

    ldp x29, x30, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x19, x20, [sp], #16
    ret

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