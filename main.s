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

    // 获取返回的实际大小
    adrp x21, buffer_size@PAGE
    add x21, x21, buffer_size@PAGEOFF
    ldr x21, [x21]  // x21 = 实际返回的大小

    // 计算结构体数量
    mov x22, #0x230  // kinfo_proc结构体大小 (可能需要根据系统调整)
    udiv x23, x21, x22  // x23 = 结构体数量

    // 初始化循环
    adrp x19, buffer@PAGE
    add x19, x19, buffer@PAGEOFF  // x19 = buffer起始地址
    mov x24, #0  // 计数器

_loop:
    cmp x24, x23
    b.ge _exit

    // 打印进程名称
    adrp x0, process_name_msg@PAGE
    add x0, x0, process_name_msg@PAGEOFF
    add x1, x19, #0x1ac  // p_comm在kinfo_proc中的偏移 (可能需要根据系统调整)
    bl _printf

    add x19, x19, x22  // 移动到下一个结构体
    add x24, x24, #1   // 增加计数器
    b _loop

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
process_name_msg:
    .asciz "进程名称: %s\n"