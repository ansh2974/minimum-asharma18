#ifndef MINIMUM_UART_H
#define MINIMUM_UART_H

#include <stdint.h>

#include "minemu/platform.h"

/* Enable UART0 receive interrupts on the device and the interrupt controller.
 * CPU interrupts must be enabled separately with minemu_irq_enable(). */
void uart_init(void);

/* Blocking byte output on the console UART (UART0). */
void uart_putc(char c);

/* Write every byte of a NUL-terminated string. */
void uart_puts(const char *s);

/* Non-blocking read of up to n bytes already received on uart. UART0 input
 * comes from the interrupt-fed buffer, so call this with IRQs masked (as in
 * a syscall). Returns the number of bytes stored in dst. */
uint32_t uart_read(volatile struct minemu_uart_regs *uart, char *dst, uint32_t n);

/* Blocking write of n bytes to uart. */
void uart_write(volatile struct minemu_uart_regs *uart, const char *src, uint32_t n);

/* MINEMU_UART_STATUS_RX_READY if input is waiting, plus
 * MINEMU_UART_STATUS_TX_READY if the transmitter can take a byte. */
uint32_t uart_status(volatile struct minemu_uart_regs *uart);

/* UART0 interrupt handler, called by minemu_irq_dispatch. */
void uart_irq_handler(void);

#endif
