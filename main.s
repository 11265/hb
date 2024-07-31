.global _main                                  // 声明_main为全局符号
.align 2                                       // 确保代码按4字节对齐

_main:                                         // _main函数入口点
    stp x29, x30, [sp, #-16]!                  // 保存帧指针和返回地址
    mov x29, sp                                // 设置新的帧指针

    bl _c_main                                 // 分支并链接到_c_main函数

    ldp x29, x30, [sp], #16                    // 恢复帧指针和返回地址
    ret                                        // 返回

.extern _c_main                                // 声明_c_main为外部符号