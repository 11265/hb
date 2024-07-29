.global _main
.align 2

_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    // 例如，读取 PID 为 22496 的进程中地址 0x102DD2404 处的 32 位整数
    mov x0, #22496       // target_pid
    
    // 使用 ADRP 和 ADD 加载大的立即数到 x1
    adrp x1, target_address@PAGE
    add x1, x1, target_address@PAGEOFF

    bl _read_int32

    ldp x29, x30, [sp], #16
    mov x0, #0
    ret

.extern _read_int32

.section __DATA,__const
target_address:
    .quad 0x102DD2404