.section __TEXT,__text
.globl _main
.p2align 2
_main:
    // 打印消息
    mov x0, #1                     // 文件描述符: stdout
    adrp x1, message@PAGE          // 获取 message 的页地址
    add x1, x1, message@PAGEOFF    // 添加页内偏移
    mov x2, #21                    // 消息长度 (包括换行符)
    mov x16, #4                    // syscall: write
    svc #0x80                      // 进行系统调用

    // 获取进程 ID
    mov x16, #20                   // syscall: getpid
    svc #0x80                      // 进行系统调用

    // 退出程序
    mov x16, #1                    // syscall: exit
    mov x0, #0                     // 退出码
    svc #0x80                      // 进行系统调用

.section __DATA,__data
message:
    .asciz "Hello, iOS Assembly!\n"