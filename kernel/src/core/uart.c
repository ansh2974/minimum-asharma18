#include "minemu/platform.h"
#include "minimum/uart.h"

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
