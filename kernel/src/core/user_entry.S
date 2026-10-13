.syntax unified
.cpu cortex-a9
.arm

@ First entry into USR mode, adapted from examples/user-mode-hello/kernel/user.S.

.global minemu_enter_user
.type minemu_enter_user, %function

@ r0 = A32 entry VA, r1 = aligned top of the user stack.
minemu_enter_user:
    cpsid if
    mov r2, r0

    @ set the banked USR sp (= r1) and lr (= 0) with a user-register ldm
    sub sp, sp, #8
    str r1, [sp]
    mov r3, #0
    str r3, [sp, #4]
    mov r3, sp
    ldmia r3, {sp, lr}^
    add sp, sp, #8

    @ spsr = USR mode, ARM state, IRQ and FIQ unmasked
    mov r3, #0x10
    msr spsr_cxsf, r3

    @ clear argument registers so no kernel values leak to user mode
    mov lr, r2
    mov r0, #0
    mov r1, #0
    mov r2, #0
    mov r3, #0
    movs pc, lr
