.section __TEXT,__text
.globl _get_pid
.globl _get_pid_by_name
.p2align 2

// 获取当前进程 PID 的函数
_get_pid:
    mov x16, #20                   // 系统调用号：getpid
    svc #0x80                      // 进行系统调用
    ret                            // 返回，PID 保存在 x0 中

// 通过进程名称获取 PID 的函数
_get_pid_by_name:
    // 保存调用者保存的寄存器
    stp x19, x20, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x27, x28, [sp, #-16]!
    stp x29, x30, [sp, #-16]!

    // 保存进程名称指针
    mov x19, x0

    // 调用 sysctl 获取进程列表
    sub sp, sp, #32                // 为 sysctl 参数分配栈空间
    mov x0, sp                     // mib 数组指针
    mov w1, #4                     // mib 数组长度
    add x2, sp, #16                // oldp 指针
    add x3, sp, #24                // oldlenp 指针
    mov x4, #0                     // newp
    mov x5, #0                     // newlen

    // 设置 mib 数组
    mov w6, #1
    str w6, [x0]                   // CTL_KERN
    mov w6, #14
    str w6, [x0, #4]               // KERN_PROC
    mov w6, #0
    str w6, [x0, #8]               // KERN_PROC_ALL
    str w6, [x0, #12]              // 0

    // 设置初始 oldlen
    mov x6, #0
    str x6, [x3]

    // 第一次调用 sysctl 获取需要的缓冲区大小
    mov x16, #202                  // sysctl 系统调用号
    svc #0x80

    // 分配缓冲区
    ldr x20, [x3]                  // 获取需要的缓冲区大小
    sub sp, sp, x20                // 在栈上分配缓冲区
    mov x2, sp                     // 更新 oldp
    str x20, [x3]                  // 更新 oldlen

    // 第二次调用 sysctl 获取实际数据
    mov x16, #202
    svc #0x80

    // 遍历进程列表
    mov x21, sp                    // 进程列表起始地址
    ldr x22, [x3]                  // 进程列表总大小
    add x22, x21, x22              // 进程列表结束地址

_process_loop:
    cmp x21, x22
    bge _not_found

    ldr w23, [x21, #44]            // 加载 PID (假设 PID 偏移为 44)
    add x24, x21, #392             // 进程名称偏移 (假设为 392)

    // 比较进程名称
    mov x0, x19                    // 目标进程名称
    mov x1, x24                    // 当前进程名称
    bl _strcmp

    cbz x0, _found                 // 如果相等 (返回值为 0)，跳转到 _found

    add x21, x21, #496             // 移动到下一个进程 (假设每个进程结构体大小为 496)
    b _process_loop

_found:
    mov x0, x23                    // 返回找到的 PID
    b _cleanup

_not_found:
    mov x0, #-1                    // 返回 -1 表示未找到

_cleanup:
    // 恢复栈和寄存器
    add sp, sp, x20
    add sp, sp, #32
    ldp x29, x30, [sp], #16
    ldp x27, x28, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x19, x20, [sp], #16
    ret

// 简单的字符串比较函数
_strcmp:
    ldrb w2, [x0], #1
    ldrb w3, [x1], #1
    cmp w2, w3
    bne _strcmp_ne
    cbnz w2, _strcmp
    mov x0, #0
    ret
_strcmp_ne:
    sub x0, x2, x3
    ret