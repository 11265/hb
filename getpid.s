.section __TEXT,__text
.globl _main
.p2align 2
_main:
    mov x0, 1                      // File descriptor: stdout
    mov x1, message                // Message to print
    mov x2, 13                     // Length of message
    movz x16, 0x2000, lsl #16
    movk x16, 0x0004
    svc 0                          // Make the syscall
    movz x16, #0x1
    movk x16, #0x200, lsl #16
    mov x0, 0                      // Exit code
    svc 0                          // Make the syscall

.section __TEXT,__data
message:
    .asciz "Hello, iOS Assembly!\n"
