.section __TEXT,__text
.globl _main
.globl _get_pid
.p2align 2

// 主程序入口
_main:
    // 打印消息
    mov x0, #1                     // 文件描述符: stdout
    adrp x1, message@PAGE          // 获取 message 的页地址
    add x1, x1, message@PAGEOFF    // 添加页内偏移
    mov x2, #21                    // 消息长度 (包括换行符)
    mov x16, #4                    // syscall: write
    svc #0x80                      // 进行系统调用

    // 调用获取进程 ID 的函数
    bl _get_pid                    // 调用 _get_pid 函数

    // 获取的进程 ID 现在在 x0 中，可以在此处进行其他操作

    // 退出程序
    mov x16, #1                    // syscall: exit
    mov x0, #0                     // 退出码
    svc #0x80                      // 进行系统调用

// 获取进程 ID 的函数
_get_pid:
    mov x16, #20                   // syscall: getpid
    svc #0x80                      // 进行系统调用
    ret                            // 返回，进程 ID 保存在 x0 中

.section __DATA,__data
message:
    .asciz "Hello, iOS Assembly!\n"
