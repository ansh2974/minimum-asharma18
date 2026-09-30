#include <stdarg.h>
#include <stdint.h>

#include "minimum/console.h"
#include "minimum/uart.h"

static void print_unsigned(uint32_t value, uint32_t base) {
    const char digits[] = "0123456789abcdef";
    char buf[10];
    int n = 0;

    do {
        buf[n++] = digits[value % base];
        value /= base;
    } while (value != 0);

    while (n > 0) {
        uart_putc(buf[--n]);
    }
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (; *fmt != '\0'; ++fmt) {
        if (*fmt != '%') {
            uart_putc(*fmt);
            continue;
        }

        ++fmt;
        switch (*fmt) {
        case 'c':
            uart_putc((char)va_arg(args, int));
            break;
        case 's': {
            const char *s = va_arg(args, const char *);
            uart_puts(s != 0 ? s : "(null)");
            break;
        }
        case 'd': {
            int32_t value = va_arg(args, int32_t);
            if (value < 0) {
                uart_putc('-');
                print_unsigned((uint32_t)0 - (uint32_t)value, 10);
            } else {
                print_unsigned((uint32_t)value, 10);
            }
            break;
        }
        case 'u':
            print_unsigned(va_arg(args, uint32_t), 10);
            break;
        case 'x':
            print_unsigned(va_arg(args, uint32_t), 16);
            break;
        case '%':
            uart_putc('%');
            break;
        case '\0':
            /* lone '%' at the end of the format string */
            va_end(args);
            return;
        default:
            /* unknown conversion: print it unchanged */
            uart_putc('%');
            uart_putc(*fmt);
            break;
        }
    }

    va_end(args);
}
