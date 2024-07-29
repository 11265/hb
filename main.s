.global _main
.align 2

_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    bl _read_process_memory

    ldp x29, x30, [sp], #16
    mov x0, #0
    ret

.extern _read_process_memory