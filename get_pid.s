.section __TEXT,__text
.globl _get_pid
.p2align 2

// 获取 PID 的函数
_get_pid:
    mov x16, #20                   // 系统调用号：getpid
    svc #0x80                      // 进行系统调用
    ret                            // 返回，PID 保存在 x0 中
