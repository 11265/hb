.global _main
.align 2

_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    // 调用我们的 read_memory 函数
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

    // 目标 PID (替换为实际的 PID)
    mov x1, #22496

    // 用于存储目标任务的指针
    sub sp, sp, #16
    mov x2, sp

    // 调用 task_for_pid
    bl _task_for_pid

    // 检查结果
    cbnz x0, _exit

    // 加载目标任务
    ldr x20, [sp]

    // 加载要读取的地址
    adrp x1, target_address
    ldr x1, [x1, #:lo12:target_address]

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
    cbnz x0, _exit

    // 在此处打印或处理读取到的数据
    // 例如，我们可以打印读取的字节数
    ldr x0, [sp]
    bl _printf

    // 如果想打印读取的数据，可以添加类似下面的代码
    // ldr x0, [sp, #8]  // 加载读取数据的地址
    // mov x1, #16       // 打印16个字节
    // bl _print_memory  // 调用一个打印内存的函数（需要自己实现）

_exit:
    mov x0, #0
    ldp x29, x30, [sp], #40
    ret

.data
target_address: .quad 0x102DD2404

.extern _mach_task_self
.extern _task_for_pid
.extern _vm_read
.extern _printf

// 如果想实现打印内存的函数，可以添加类似下面的代码
// _print_memory:
//     // 实现打印内存的逻辑
//     ret