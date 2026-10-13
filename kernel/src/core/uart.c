#include "minemu/platform.h"
#include "minimum/uart.h"

/* same size as the UART hardware receive buffer */
#define RX_BUFFER_SIZE MINEMU_UART_RX_CAPACITY

/* UART0 receive ring buffer, filled by uart_irq_handler (IRQ mode) and
 * drained by uart_read (SVC mode, IRQs masked). */
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

static void put_byte(volatile struct minemu_uart_regs *uart, char c) {
    /* wait until the transmitter can accept another byte */
    while (!(uart->status & MINEMU_UART_STATUS_TX_READY)) {
    }
    uart->tx_data = (uint32_t)(uint8_t)c;
}

void uart_putc(char c) {
    put_byte(MINEMU_UART0, c);
}

void uart_puts(const char *s) {
    while (*s != '\0') {
        uart_putc(*s);
        ++s;
    }
}

/* Move every byte waiting in the UART0 hardware FIFO into rx_buffer. */
static void drain_uart0(void) {
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

void uart_irq_handler(void) {
    /* drain every byte so the RX interrupt condition clears */
    drain_uart0();
}

uint32_t uart_read(volatile struct minemu_uart_regs *uart, char *dst, uint32_t n) {
    uint32_t got = 0;

    if (uart == MINEMU_UART0) {
        /* bytes that arrived before RX interrupts were enabled raise no
         * interrupt, so also collect anything still in the FIFO */
        drain_uart0();
        while (got < n && rx_count > 0) {
            dst[got++] = rx_buffer[rx_tail];
            rx_tail = (rx_tail + 1) % RX_BUFFER_SIZE;
            --rx_count;
        }
        return got;
    }

    /* other UARTs are not interrupt driven: poll the device directly */
    while (got < n && (uart->status & MINEMU_UART_STATUS_RX_READY)) {
        dst[got++] = (char)(uart->rx_data & 0xff);
    }
    return got;
}

void uart_write(volatile struct minemu_uart_regs *uart, const char *src, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) {
        put_byte(uart, src[i]);
    }
}

uint32_t uart_status(volatile struct minemu_uart_regs *uart) {
    if (uart == MINEMU_UART0) {
        drain_uart0();
    }
    uint32_t hw = uart->status;
    uint32_t status = hw & MINEMU_UART_STATUS_TX_READY;

    if (uart == MINEMU_UART0 ? rx_count > 0 : (hw & MINEMU_UART_STATUS_RX_READY) != 0) {
        status |= MINEMU_UART_STATUS_RX_READY;
    }
    return status;
}
