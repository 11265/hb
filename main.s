.global _main
.align 2

_main:
    // 保存链接寄存器和帧指针
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp

    // 打印开始信息
    adrp    x0, start_msg@PAGE
    add     x0, x0, start_msg@PAGEOFF
    bl      _printf

    // 调用 get_pvz_pid 函数
    bl      _get_pvz_pid

    // 加载全局变量 global_pid 的地址
    adrp    x0, _global_pid@PAGE
    add     x0, x0, _global_pid@PAGEOFF

    // 打印全局变量的地址
    mov     x1, x0
    adrp    x0, addr_msg@PAGE
    add     x0, x0, addr_msg@PAGEOFF
    bl      _printf

    // 读取全局变量的值
    ldr     w1, [x0]

    // 打印汇编中读取的全局变量值
    adrp    x0, global_msg@PAGE
    add     x0, x0, global_msg@PAGEOFF
    bl      _printf

    // 打印结束信息
    adrp    x0, end_msg@PAGE
    add     x0, x0, end_msg@PAGEOFF
    bl      _printf

    // 恢复链接寄存器和帧指针，并返回
    ldp     x29, x30, [sp], #16
    mov     w0, #0
    ret

.data
start_msg:      .asciz "程序开始执行...\n"
addr_msg:       .asciz "汇编中 global_pid 的地址: %p\n"
global_msg:     .asciz "汇编中读取的全局变量值: %d\n"
end_msg:        .asciz "程序执行结束\n"