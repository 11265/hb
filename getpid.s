.section __TEXT,__text
.globl _main
.globl _get_pid
.p2align 2

_main:
    // 打印消息
    mov x0, #1
    adrp x1, message@PAGE
    add x1, x1, message@PAGEOFF
    mov x2, #21
    mov x16, #4
    svc #0x80

    // 调用获取进程 ID 的函数
    bl _get_pid

    // 保存 PID
    mov x19, x0

    // 打印 "PID: " 消息
    mov x0, #1
    adrp x1, pid_message@PAGE
    add x1, x1, pid_message@PAGEOFF
    mov x2, #5
    mov x16, #4
    svc #0x80

    // 打印实际的 PID 数字
    mov x0, x19
    bl _print_number

    // 打印换行符
    mov x0, #1
    adrp x1, newline@PAGE
    add x1, x1, newline@PAGEOFF
    mov x2, #1
    mov x16, #4
    svc #0x80

    // 退出程序
    mov x16, #1
    mov x0, #0
    svc #0x80

_get_pid:
    mov x16, #20     // getpid 系统调用号
    svc #0x80
    
    // 保存返回值
    mov x19, x0
    
    // 打印调试信息
    mov x0, #1
    adrp x1, debug_message@PAGE
    add x1, x1, debug_message@PAGEOFF
    mov x2, #14
    mov x16, #4
    svc #0x80
    
    // 返回原始的 PID
    mov x0, x19
    ret

_print_number:
    // 打印固定的数字 "42" 进行测试
    mov x0, #1
    adrp x1, test_number@PAGE
    add x1, x1, test_number@PAGEOFF
    mov x2, #2
    mov x16, #4
    svc #0x80
    ret

.section __DATA,__data
message:
    .asciz "Hello, iOS Assembly!\n"
pid_message:
    .asciz "PID: "
newline:
    .asciz "\n"
debug_message:
    .asciz "Debug: get_pid\n"
test_number:
    .asciz "42"