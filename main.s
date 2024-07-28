.global _main
.align 2

_main:
    // 保存链接寄存器
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp

    // 打印开始信息
    adrp    x0, start_msg@PAGE
    add     x0, x0, start_msg@PAGEOFF
    bl      _printf

    // 调用 get_pvz_pid 函数
    bl      _get_pvz_pid

    // 立即打印返回值
    mov     w1, w0
    adrp    x0, return_msg@PAGE
    add     x0, x0, return_msg@PAGEOFF
    bl      _printf

    // 检查返回值是否为 -1
    cmp     w0, #-1
    b.eq    not_found

    // 打印找到进程的信息
    mov     w1, w0  // 将返回的 PID 移动到 w1 作为第二个参数
    adrp    x0, found_msg@PAGE
    add     x0, x0, found_msg@PAGEOFF
    bl      _printf
    b       end

not_found:
    // 打印未找到进程的信息
    adrp    x0, not_found_msg@PAGE
    add     x0, x0, not_found_msg@PAGEOFF
    bl      _printf

end:
    // 打印结束信息
    adrp    x0, end_msg@PAGE
    add     x0, x0, end_msg@PAGEOFF
    bl      _printf

    // 恢复链接寄存器并返回
    ldp     x29, x30, [sp], #16
    mov     w0, #0
    ret

.data
start_msg:      .asciz "程序开始执行，正在查找PVZ进程...\n"
return_msg:     .asciz "汇编中接收到的返回值: %d\n"
found_msg:      .asciz "找到PVZ进程，PID: %d\n"
not_found_msg:  .asciz "未找到PVZ进程\n"
end_msg:        .asciz "程序执行结束\n"