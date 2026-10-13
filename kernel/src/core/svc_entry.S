.syntax unified
.cpu cortex-a9
.arm

@ SVC entry trampoline, adapted from examples/user-mode-hello/kernel/svc.S.
@ Builds a struct minemu_trap_frame (88 bytes) on the SVC stack, calls
@ minemu_svc_dispatch(frame), then restores from the frame it returns.
@
@ Trap-frame offsets (see minemu/trap.h):
@   0  r0-r12      52 return_lr   56 spsr      60 exception_id
@   64 fault_pc    68 dfsr        72 dfar      76 user_sp
@   80 user_lr     84 reserved

.global minemu_svc_trampoline
.type minemu_svc_trampoline, %function
.extern minemu_svc_dispatch

minemu_svc_trampoline:
    @ 1. allocate one trap frame on the SVC stack
    sub sp, sp, #88

    @ 2. save r0-r12, LR_svc (instruction after svc) and SPSR_svc (caller's cpsr)
    stmia sp, {r0-r12}
    str lr, [sp, #52]
    mrs r1, spsr
    str r1, [sp, #56]

    @ 3. exception_id = MINEMU_EXCEPTION_SVC
    ldr r1, =-3
    str r1, [sp, #60]

    @ 4. fault_pc = address of the svc instruction itself
    sub r1, lr, #4
    str r1, [sp, #64]

    @ no fault status for a syscall
    mov r1, #0
    str r1, [sp, #68]
    str r1, [sp, #72]

    @ 5. save the banked USR sp and lr
    add r1, sp, #76
    stmia r1, {sp, lr}^
    mov r1, #0
    str r1, [sp, #84]

    @ 6. call minemu_svc_dispatch(frame); it writes the result into frame->r[0]
    mov r0, sp
    bl minemu_svc_dispatch

    @ 7. restore the context from the frame returned by the dispatcher
    mov sp, r0
    ldr lr, [sp, #52]
    ldr r1, [sp, #56]
    msr spsr_cxsf, r1
    add r1, sp, #76
    ldmia r1, {sp, lr}^
    ldmia sp, {r0-r12}
    add sp, sp, #88

    @ 8. return to the instruction after svc and restore cpsr from spsr
    movs pc, lr
