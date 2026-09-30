#include "minemu/irq.h"
#include "minemu/platform.h"
#include "minimum/uart.h"

/* same size as the UART hardware receive buffer */
#define RX_BUFFER_SIZE MINEMU_UART_RX_CAPACITY

/* Receive ring buffer shared between uart_irq_handler (IRQ mode) and
 * uart_getc (kernel main program). The main program only touches it
 * with interrupts disabled. */
static char rx_buffer[RX_BUFFER_SIZE];
static uint32_t rx_head; /* next slot to write */
static uint32_t rx_tail; /* next slot to read */
static uint32_t rx_count;

void uart_init(void) {
    rx_head = 0;
    rx_tail = 0;
    rx_count = 0;
    MINEMU_UART0->control = MINEMU_UART_CONTROL_RX_IRQ_ENABLE;
    MINEMU_INTERRUPT->enable = UINT32_C(1) << MINEMU_IRQ_UART0;
}

void uart_putc(char c) {
    /* wait until the transmitter can accept another byte */
    while (!(MINEMU_UART0->status & MINEMU_UART_STATUS_TX_READY)) {
    }
    MINEMU_UART0->tx_data = (uint32_t)(uint8_t)c;
}

void uart_puts(const char *s) {
    while (*s != '\0') {
        uart_putc(*s);
        ++s;
    }
}

void uart_irq_handler(void) {
    /* drain every byte so the RX interrupt condition clears */
    while (MINEMU_UART0->status & MINEMU_UART_STATUS_RX_READY) {
        char c = (char)(MINEMU_UART0->rx_data & 0xff);
        if (rx_count < RX_BUFFER_SIZE) {
            rx_buffer[rx_head] = c;
            rx_head = (rx_head + 1) % RX_BUFFER_SIZE;
            ++rx_count;
        }
        /* buffer full: the byte is dropped */
    }
}

char uart_getc(void) {
    char c;

    for (;;) {
        minemu_irq_disable();
        if (rx_count > 0) {
            c = rx_buffer[rx_tail];
            rx_tail = (rx_tail + 1) % RX_BUFFER_SIZE;
            --rx_count;
            minemu_irq_enable();
            return c;
        }
        /* nothing yet: re-enable interrupts so a pending UART interrupt
         * can fill the buffer, then check again */
        minemu_irq_enable();
    }
}
