.syntax unified
.cpu cortex-a9
.arm

@ IRQ entry trampoline, adapted from examples/irq-context-switch/irq.S.
@ Builds a struct minemu_trap_frame (88 bytes) on the IRQ stack, calls
@ minemu_irq_dispatch(frame), then restores from the frame it returns.
@
@ Trap-frame offsets (see minemu/trap.h):
@   0  r0-r12      52 return_lr   56 spsr      60 exception_id
@   64 fault_pc    68 dfsr        72 dfar      76 user_sp
@   80 user_lr     84 reserved

.global minemu_irq_trampoline
.type minemu_irq_trampoline, %function
.extern minemu_irq_dispatch

minemu_irq_trampoline:
    @ 1. allocate one trap frame on the IRQ stack
    sub sp, sp, #88

    @ 2. save r0-r12, the IRQ lr and the interrupted program's status (spsr)
    stmia sp, {r0-r12}
    str lr, [sp, #52]
    mrs r1, spsr
    str r1, [sp, #56]

    @ 3. read the interrupt controller claim register (MINEMU_INTERRUPT_BASE + 8)
    @    and store the active source id in exception_id
    ldr r1, =0x10000000
    ldr r1, [r1, #8]
    str r1, [sp, #60]

    @ 4. interrupted instruction address: the IRQ lr is PC+4
    sub r1, lr, #4
    str r1, [sp, #64]

    @ dfsr / dfar are not used for IRQs
    mov r1, #0
    str r1, [sp, #68]
    str r1, [sp, #72]

    @ 5. save the banked (user) sp and lr
    add r1, sp, #76
    stmia r1, {sp, lr}^
    mov r1, #0
    str r1, [sp, #84]

    @ 6. call minemu_irq_dispatch(frame)
    mov r0, sp
    bl minemu_irq_dispatch

    @ 7. restore the context from the frame returned by the dispatcher
    mov sp, r0
    ldr lr, [sp, #52]
    ldr r1, [sp, #56]
    msr spsr_cxsf, r1
    add r1, sp, #76
    ldmia r1, {sp, lr}^
    ldmia sp, {r0-r12}
    add sp, sp, #88

    @ 8. return to the interrupted instruction and restore cpsr from spsr
    subs pc, lr, #4
