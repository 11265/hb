.global _main
.align 2

_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    bl _read_memory

    mov x0, #0
    ldp x29, x30, [sp], #16
    ret

.global _read_memory
_read_memory:
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    // 获取当前任务
    mov x0, #-1  // mach_task_self()
    bl _mach_task_self

    // 存储任务端口
    mov x19, x0

    // 使用自己的任务端口
    mov x20, x19

    // 要读取的地址（这里使用一个可能有效的地址，比如程序本身的地址）
    adr x1, _main

    // 要读取的大小
    mov x2, #16

    // 用于存储读取数据的指针
    sub sp, sp, #16
    mov x3, sp

    // 用于存储读取字节数的指针
    sub sp, sp, #8
    mov x4, sp

    // 调用 vm_read
    mov x0, x20
    bl _vm_read

    // 检查结果
    cbnz x0, _print_error

    // 打印读取的字节数
    ldr x1, [sp]
    adr x0, read_msg
    bl _printf

    b _exit

_print_error:
    // 打印错误信息
    mov x1, x0
    adr x0, error_msg
    bl _printf

_exit:
    mov x0, #0
    ldp x29, x30, [sp], #40
    ret

.data
read_msg: .asciz "Read %d bytes\n"
error_msg: .asciz "Error: %d\n"

.extern _mach_task_self
.extern _vm_read
.extern _printf