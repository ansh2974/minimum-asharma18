#include "minemu/user_abi.h"

.syntax unified
.cpu cortex-a9
.arm

@ int32_t ioctl(int fd, uint32_t request, ...);
@
@ AAPCS passes the first four words in r0-r3 even for variadic calls, so fd,
@ request and up to two extra arguments are already where the kernel expects
@ them. r7 carries the syscall number and is callee-saved, so preserve it.

.global ioctl
.type ioctl, %function

ioctl:
    push {r7, lr}
    mov r7, #MINEMU_SYSCALL_IOCTL
    svc #0
    pop {r7, pc}
