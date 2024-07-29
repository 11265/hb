.global _main
.align 2

_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    // 调用 c_main 函数
    bl _c_main

    // c_main 的返回值已经在 x0 中，无需额外处理

    ldp x29, x30, [sp], #16
    ret

.extern _c_main