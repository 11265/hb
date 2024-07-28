.section __DATA,__data
.align 3
mib:
    .long 1, 14, 0, 0, 0  // CTL_KERN, KERN_PROC, KERN_PROC_ALL
buffer:
    .space 1024*1024  // 1MB用于进程信息的缓冲区
buffer_size:
    .quad 1024*1024
process_name:
    .asciz "pvz"  // 要查找的进程名称

.section __TEXT,__text
.globl _main
.align 2

_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    // 调用sysctl获取进程列表
    adrp x0, mib@PAGE
    add x0, x0, mib@PAGEOFF
    mov x1, #5  // mib数组的长度
    adrp x2, buffer@PAGE
    add x2, x2, buffer@PAGEOFF
    adrp x3, buffer_size@PAGE
    add x3, x3, buffer_size@PAGEOFF
    mov x4, #0
    mov x5, #0
    mov x16, #202  // sysctl系统调用号
    svc #0x80

    // 检查返回值
    cmp x0, #0
    b.lt _error  // 如果小于零（发生错误）则分支

    // 打印成功消息
    adrp x0, success_msg@PAGE
    add x0, x0, success_msg@PAGEOFF
    bl _printf

    // 处理缓冲区以查找指定进程的PID
    adrp x19, buffer@PAGE
    add x19, x19, buffer@PAGEOFF  // x19 = buffer起始地址
    adrp x20, buffer_size@PAGE
    add x20, x20, buffer_size@PAGEOFF
    ldr x20, [x20]  // x20 = buffer实际大小
    add x20, x19, x20  // x20 = buffer结束地址

_loop:
    cmp x19, x20
    b.ge _not_found

    // 比较进程名
    add x0, x19, #0x1ac  // p_comm在kinfo_proc中的偏移
    adrp x1, process_name@PAGE
    add x1, x1, process_name@PAGEOFF
    bl _strcmp

    cmp x0, #0
    b.eq _found

    add x19, x19, #0x230  // kinfo_proc结构体大小
    b _loop

_found:
    // 打印找到的PID
    adrp x0, found_msg@PAGE
    add x0, x0, found_msg@PAGEOFF
    ldr w1, [x19, #0x68]  // p_pid在kinfo_proc中的偏移
    bl _printf
    b _exit

_not_found:
    // 打印未找到进程的消息
    adrp x0, not_found_msg@PAGE
    add x0, x0, not_found_msg@PAGEOFF
    bl _printf
    b _exit

_error:
    // 获取errno（sysctl调用后已经在x0中）
    mov x19, x0  // 保存errno

    // 打印errno
    adrp x0, error_msg@PAGE
    add x0, x0, error_msg@PAGEOFF
    mov x1, x19
    bl _printf

_exit:
    mov x0, #0
    mov x16, #1  // exit系统调用
    svc #0x80

.section __TEXT,__cstring
error_msg:
    .asciz "sysctl调用失败，errno: %d\n"
success_msg:
    .asciz "sysctl调用成功\n"
found_msg:
    .asciz "找到进程，PID: %d\n"
not_found_msg:
    .asciz "未找到指定的进程\n"