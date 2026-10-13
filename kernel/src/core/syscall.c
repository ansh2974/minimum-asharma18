#include <stdbool.h>
#include <stdint.h>

#include "minemu/mmu.h"
#include "minemu/platform.h"
#include "minemu/syscall.h"
#include "minemu/trace.h"
#include "minemu/trap.h"
#include "minimum/ioctl.h"
#include "minimum/uart.h"
#include "minimum/vm.h"

#define CPSR_MODE_MASK UINT32_C(0x1f)
#define CPSR_MODE_USR UINT32_C(0x10)
#define CPSR_THUMB UINT32_C(0x20)
#define A32_SVC_ZERO UINT32_C(0xef000000)

#define ERROR MINEMU_IOCTL_ERROR

enum device {
    DEVICE_NONE,
    DEVICE_RNG,
    DEVICE_UART0,
    DEVICE_UART1,
    DEVICE_TRACE,
};

/* Both fd numberings (see minimum/ioctl.h) name the same four devices. */
static enum device device_from_fd(uint32_t fd) {
    switch (fd) {
    case MINEMU_FD_UART_IN:
    case MINEMU_FD_UART_OUT:
    case IOCTL_FD_UART0:
        return DEVICE_UART0;
    case MINEMU_FD_UART_DIAG:
    case IOCTL_FD_UART1:
        return DEVICE_UART1;
    case MINEMU_FD_RNG:
    case IOCTL_FD_RNG:
        return DEVICE_RNG;
    case MINEMU_FD_TRACE:
    case IOCTL_FD_TRACE:
        return DEVICE_TRACE;
    default:
        return DEVICE_NONE;
    }
}

/* Store a 32-bit value at a validated user address. Byte stores so an
 * unaligned pointer cannot fault in the kernel. */
static void put_user_u32(uint32_t vaddr, uint32_t value) {
    volatile uint8_t *out = (volatile uint8_t *)(uintptr_t)vaddr;
    for (uint32_t i = 0; i < 4U; ++i) {
        out[i] = (uint8_t)(value >> (8U * i));
    }
}

static int32_t rng_ioctl(uint32_t op, uint32_t arg) {
    uint32_t value;

    switch (op) {
    case IOCTL_RNG_SET_SEED:
    case MINEMU_IOCTL_RNG_SEED:
        MINEMU_RNG->seed = arg;
        return 0;
    case IOCTL_RNG_GET_DATA:
    case MINEMU_IOCTL_RNG_NEXT:
        /* validate before reading DATA, which advances the generator */
        if (!vm_user_range_ok(arg, 4U, MINEMU_PTE_WRITABLE)) {
            return ERROR;
        }
        value = MINEMU_RNG->data;
        put_user_u32(arg, value);
        return 0;
    case MINEMU_IOCTL_RNG_STATE:
        if (!vm_user_range_ok(arg, 4U, MINEMU_PTE_WRITABLE)) {
            return ERROR;
        }
        value = MINEMU_RNG->state;
        put_user_u32(arg, value);
        return 0;
    default:
        return ERROR;
    }
}

static int32_t uart_ioctl(volatile struct minemu_uart_regs *uart, uint32_t op, uint32_t buf,
                          uint32_t count) {
    int32_t n = (int32_t)count;

    switch (op) {
    case IOCTL_UART_READ:
    case MINEMU_IOCTL_UART_READ:
        /* validate the whole destination before consuming any input */
        if (n < 0 || !vm_user_range_ok(buf, (uint32_t)n, MINEMU_PTE_WRITABLE)) {
            return ERROR;
        }
        return (int32_t)uart_read(uart, (char *)(uintptr_t)buf, (uint32_t)n);
    case IOCTL_UART_WRITE:
    case MINEMU_IOCTL_UART_WRITE:
        if (n < 0 || !vm_user_range_ok(buf, (uint32_t)n, MINEMU_PTE_READABLE)) {
            return ERROR;
        }
        uart_write(uart, (const char *)(uintptr_t)buf, (uint32_t)n);
        return n;
    case MINEMU_IOCTL_UART_STATUS:
        return (int32_t)uart_status(uart);
    default:
        return ERROR;
    }
}

static int32_t trace_ioctl(uint32_t op, uint32_t value) {
    switch (op) {
    case IOCTL_TRACE_EVENT:
    case MINEMU_IOCTL_TRACE_EVENT:
        minemu_trace_event(value);
        return 0;
    default:
        return ERROR;
    }
}

/* int ioctl(int fd, int op, ...): fd in r0, op in r1, variadic args in r2-r3. */
static int32_t sys_ioctl(const struct minemu_trap_frame *frame) {
    uint32_t op = frame->r[1];

    switch (device_from_fd(frame->r[0])) {
    case DEVICE_RNG:
        return rng_ioctl(op, frame->r[2]);
    case DEVICE_UART0:
        return uart_ioctl(MINEMU_UART0, op, frame->r[2], frame->r[3]);
    case DEVICE_UART1:
        return uart_ioctl(MINEMU_UART1, op, frame->r[2], frame->r[3]);
    case DEVICE_TRACE:
        return trace_ioctl(op, frame->r[2]);
    default:
        return ERROR;
    }
}

/* Only an A32 `svc #0` executed from USR mode is a valid syscall. */
static bool is_user_svc(const struct minemu_trap_frame *frame) {
    if (frame->exception_id != MINEMU_EXCEPTION_SVC ||
        (frame->spsr & CPSR_MODE_MASK) != CPSR_MODE_USR || (frame->spsr & CPSR_THUMB) != 0 ||
        (frame->fault_pc & 3U) != 0 ||
        !vm_user_range_ok(frame->fault_pc, 4U, MINEMU_PTE_READABLE | MINEMU_PTE_EXECUTABLE)) {
        return false;
    }
    return *(const volatile uint32_t *)(uintptr_t)frame->fault_pc == A32_SVC_ZERO;
}

struct minemu_trap_frame *minemu_svc_dispatch(struct minemu_trap_frame *frame) {
    int32_t result = ERROR;

    if (is_user_svc(frame)) {
        /* syscall number in r7 */
        switch (frame->r[7]) {
        case MINEMU_SYSCALL_IOCTL:
            result = sys_ioctl(frame);
            break;
        default:
            break;
        }
    }

    /* the trampoline restores r0 from the frame, so this is the return value */
    frame->r[0] = (uint32_t)result;
    return frame;
}
