.global _main
.align 2

_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    // 设置目标进程的 PID (22496)
    mov x0, #22496       // target_pid
    
    // 加载目标地址 0x102DD2404 到 x1
    movz x1, #0x2404, lsl #0
    movk x1, #0x02DD, lsl #16
    movk x1, #0x0001, lsl #32

    // 调用 read_int32 函数
    bl _read_int32

    // 检查 read_int32 的返回值
    cmp x0, #0
    b.ne .exit  // 如果不为 0，直接退出

    // 如果 read_int32 成功，返回 0
    mov x0, #0

.exit:
    ldp x29, x30, [sp], #16
    ret

.extern _read_int32