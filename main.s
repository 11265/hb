.section __TEXT,__text
.globl _main
.align 2

_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    // 打印开始消息
    adrp x0, start_msg@PAGE
    add x0, x0, start_msg@PAGEOFF
    bl _puts

    // 调用C函数获取PVZ的PID
    bl _get_pvz_pid

    // 检查返回值
    cmp x0, #-1
    b.eq _not_found

    // 打印结果（如果找到）
    mov x1, x0
    adrp x0, found_proc_msg@PAGE
    add x0, x0, found_proc_msg@PAGEOFF
    bl _printf
    b _exit

_not_found:
    // 打印未找到进程的消息
    adrp x0, not_found_msg@PAGE
    add x0, x0, not_found_msg@PAGEOFF
    bl _puts

_exit:
    // 打印结束消息
    adrp x0, end_msg@PAGE
    add x0, x0, end_msg@PAGEOFF
    bl _puts

    mov x0, #0
    mov x16, #1 // exit系统调用
    svc #0x80

.section __TEXT,__cstring
start_msg:
    .asciz "程序开始执行，正在查找PVZ进程..."
found_proc_msg:
    .asciz "找到PVZ进程，PID: %d\n"
not_found_msg:
    .asciz "未找到PVZ进程"
end_msg:
    .asciz "程序执行结束"