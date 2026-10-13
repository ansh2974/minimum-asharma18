#include <stdint.h>

#include "minemu/user_abi.h"

/* User-mode hello world: prints through the ioctl syscall, since USR mode
 * cannot touch the UART registers directly. */
void minemu_user_main(void) {
    static const char message[] = "hello world\n";

    (void)ioctl(MINEMU_FD_UART_OUT, MINEMU_IOCTL_UART_WRITE, (void *)message,
                (uint32_t)(sizeof(message) - 1U));

    for (;;) {
        __asm__ volatile("nop");
    }
}
