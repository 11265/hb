.global _main
.align 2

_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    // 例如，读取 PID 为 1234 的进程中地址 0x10000000 处的 32 位整数
    mov x0, #22496       // target_pid
    mov x1, #0x102DD2404 // target_address
    bl _read_int32

    ldp x29, x30, [sp], #16
    mov x0, #0
    ret

.extern _read_int32