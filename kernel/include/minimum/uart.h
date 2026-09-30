#ifndef MINIMUM_UART_H
#define MINIMUM_UART_H

/* Enable UART0 receive interrupts on the device and the interrupt controller.
 * CPU interrupts must be enabled separately with minemu_irq_enable(). */
void uart_init(void);

/* Blocking byte output on the console UART (UART0). */
void uart_putc(char c);

/* Write every byte of a NUL-terminated string. */
void uart_puts(const char *s);

/* Blocking byte input: returns the oldest received byte, waiting for
 * the receive interrupt if none has arrived yet. */
char uart_getc(void);

/* UART0 interrupt handler, called by minemu_irq_dispatch. */
void uart_irq_handler(void);

#endif
