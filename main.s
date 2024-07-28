.section __DATA,__data
KERN_SUCCESS:
    .long 0
mib:
    .long 1, 14, 0, 0  // CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0
miblen:
    .long 4
process_name:
    .asciz "pvz"  // 替换为目标进程名
buffer_size:
    .quad 4096  // 初始缓冲区大小

.section __TEXT,__text
.globl _main

.extern _sysctl
.extern _malloc
.extern _free
.extern _strcmp

_main:
    // 保存寄存器
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    // 分配内存
    ldr x0, buffer_size
    bl _malloc
    mov x19, x0  // x19 = buffer

    // 设置 sysctl 参数
    sub sp, sp, #32
    adr x0, mib
    str x0, [sp]
    adr x1, miblen
    str x1, [sp, #8]
    str x19, [sp, #16]  // buffer
    adr x2, buffer_size
    str x2, [sp, #24]  // &size

    // 调用 sysctl
    mov x0, sp
    bl _sysctl

    // 检查返回值
    cmp w0, #0
    bne .cleanup

    // 遍历进程列表
    mov x20, x19  // x20 = current process
    ldr x21, buffer_size  // x21 = buffer size
    adr x22, process_name  // x22 = target process name

.loop:
    ldr x0, [x20, #44]  // 假设进程名在偏移量44处，可能需要调整
    mov x1, x22
    bl _strcmp
    cbz w0, .found_process

    add x20, x20, #0x100  // 移动到下一个进程，假设每个条目大小为256字节
    sub x21, x21, #0x100
    cmp x21, #0
    bgt .loop

    b .not_found

.found_process:
    // 进程找到，PID 通常在结构的开始
    ldr w0, [x20]
    // 这里可以添加代码来处理找到的 PID
    b .cleanup

.not_found:
    mov w0, #-1

.cleanup:
    // 释放内存
    mov x0, x19
    bl _free

    // 恢复寄存器并返回
    ldp x29, x30, [sp], #16
    ret