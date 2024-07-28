.section __TEXT,__text
.globl _get_pid_by_name
.p2align 2

_get_pid_by_name:
    stp x19, x20, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    mov x19, x0  // Save process name pointer

    mov x0, #4096  // 4KB for PID list
    bl _malloc
    cmp x0, #0
    beq _cleanup   // Jump to cleanup if malloc fails
    mov x20, x0  // Save allocated memory pointer

    mov x0, #1  // PROC_ALL_PIDS
    mov x1, #0
    mov x2, x20  // buffer
    mov x3, #4096
    bl _proc_listpids

    cmp x0, #0
    ble _cleanup   // Jump to cleanup if proc_listpids fails or returns 0

    mov x21, x0  // Save returned byte count
    mov x22, #0  // PID counter

_pid_loop:
    ldr w0, [x20, x22, lsl #2]  // Load PID
    
    sub sp, sp, #1024
    mov x1, sp
    mov x2, #1024
    bl _proc_name

    mov x0, x19  // Target process name
    mov x1, sp   // Current process name
    bl _strcmp

    add sp, sp, #1024

    cbz w0, _found_pid  // If matching process name found

    add x22, x22, #1
    lsl x0, x22, #2
    cmp x0, x21
    blt _pid_loop

    mov x0, #0  // Process not found
    b _cleanup

_found_pid:
    ldr w0, [x20, x22, lsl #2]  // Return found PID

_cleanup:
    cmp x20, #0    // Check if buffer pointer is NULL
    beq _exit      // If NULL, skip freeing
    mov x1, x20
    bl _free
    mov x20, #0    // Set to NULL after freeing

_exit:
    ldp x29, x30, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x19, x20, [sp], #16
    ret

.section __DATA,__data