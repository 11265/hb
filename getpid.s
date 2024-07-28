.section __TEXT,__text
.globl _main
.p2align 2
_main:
    mov x0, 1                      // File descriptor: stdout
    mov x1, message                // Message to print
    mov x2, 13                     // Length of message
    mov x16, 0x2000004             // syscall number for write
    svc 0                          // Make the syscall
    mov x16, 0x2000001             // syscall number for exit
    mov x0, 0                      // Exit code
    svc 0                          // Make the syscall

.section __TEXT,__data
message:
    .asciz "Hello, iOS Assembly!\n"
