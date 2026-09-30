#ifndef MINIMUM_UART_H
#define MINIMUM_UART_H

/* Blocking byte output on the console UART (UART0). */
void uart_putc(char c);

/* Write every byte of a NUL-terminated string. */
void uart_puts(const char *s);

#endif
