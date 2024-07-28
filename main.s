.section __TEXT,__text
.globl _main
.p2align 2

_main:
    // 打印欢迎消息
    mov x0, #1
    adrp x1, message@PAGE
    add x1, x1, message@PAGEOFF
    mov x2, #15  // 修改为正确的长度
    mov x16, #4
    svc #0x80

    // 获取当前进程 ID
    bl _get_pid
    mov x20, x0  // 保存当前进程 ID

    // 打印当前进程 ID 消息
    mov x0, #1
    adrp x1, current_pid_message@PAGE
    add x1, x1, current_pid_message@PAGEOFF
    mov x2, #13  // 确保这个长度是正确的
    mov x16, #4
    svc #0x80

    // 打印当前进程 ID 数字
    mov x0, x20
    bl _print_number

    // 打印换行符
    bl _print_newline

    // 通过名称获取指定进程 ID
    adrp x0, process_name@PAGE
    add x0, x0, process_name@PAGEOFF
    bl _get_pid_by_name

    // 检查返回值
    cmp x0, #-1
    beq _not_found

    // 保存找到的 PID
    mov x19, x0

    // 打印找到的进程 ID 消息
    mov x0, #1
    adrp x1, found_pid_message@PAGE
    add x1, x1, found_pid_message@PAGEOFF
    mov x2, #21
    mov x16, #4
    svc #0x80

    // 打印找到的进程 ID 数字
    mov x0, x19
    bl _print_number

    b _exit_program

_not_found:
    // 打印未找到进程的消息
    mov x0, #1
    adrp x1, not_found_message@PAGE
    add x1, x1, not_found_message@PAGEOFF
    mov x2, #36  // 增加长度以确保完整显示
    mov x16, #4
    svc #0x80

_exit_program:
    // 打印换行符
    bl _print_newline

    // 退出程序
    mov x16, #1
    mov x0, #0
    svc #0x80

_print_newline:
    mov x0, #1
    adrp x1, newline@PAGE
    add x1, x1, newline@PAGEOFF
    mov x2, #1
    mov x16, #4
    svc #0x80
    ret

.section __DATA,__data
message:
    .asciz "iOS Assembly!\n"
current_pid_message:
    .asciz "Current PID: "
found_pid_message:
    .asciz "Found process PID: "
not_found_message:
    .asciz "Process not found or error occurred\n"
newline:
    .asciz "\n"
process_name:
    .asciz "pvz"  // 或者尝试 "CommCenter"