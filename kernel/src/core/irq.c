#include "minemu/irq.h"
#include "minemu/platform.h"
#include "minimum/uart.h"

#define IRQ_SOURCE_COUNT 4

typedef void (*irq_handler_t)(void);

/* Handler table indexed by interrupt source ID. */
static const irq_handler_t irq_handlers[IRQ_SOURCE_COUNT] = {
    [MINEMU_IRQ_SYSTICK] = 0,
    [MINEMU_IRQ_UART0] = uart_irq_handler,
    [MINEMU_IRQ_UART1] = 0,
    [MINEMU_IRQ_BLOCK] = 0,
};

struct minemu_trap_frame *minemu_irq_dispatch(struct minemu_trap_frame *frame) {
    uint32_t source = (uint32_t)frame->exception_id;

    /* no active source (spurious interrupt): nothing to handle or acknowledge */
    if (source >= IRQ_SOURCE_COUNT) {
        return frame;
    }

    if (irq_handlers[source] != 0) {
        irq_handlers[source]();
    }

    /* end of interrupt for the active source */
    MINEMU_INTERRUPT->eoi = source;

    return frame;
}
