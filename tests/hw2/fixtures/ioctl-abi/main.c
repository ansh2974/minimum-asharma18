#include <stdint.h>

#include "minimum/ioctl.h"

/* Exercises the assignment-table ioctl numbering (fd = device base address,
 * op = 0/1) and the kernel's argument validation. Each result is reported as
 * a trace event; see ioctl-abi.toml for the expected sequence. */

#define KERNEL_ADDRESS UINT32_C(0xc0030000)     /* mapped, supervisor only */
#define SYSTEM_ROM_ADDRESS UINT32_C(0x08000000) /* mapped, supervisor read-only */

static void trace(uint32_t value)
{
  (void)ioctl(IOCTL_FD_TRACE, IOCTL_TRACE_EVENT, value);
}

/* svc #0 with an arbitrary syscall number in r7 */
static int32_t raw_syscall(uint32_t number)
{
  register uint32_t r0 __asm__("r0") = 0;
  register uint32_t r7 __asm__("r7") = number;
  __asm__ volatile("svc #0" : "+r"(r0) : "r"(r7) : "memory");
  return (int32_t)r0;
}

void minemu_user_main(void)
{
  static char message[] = "abi ok\n";
  uint32_t value = 0;

  /* 1. UART0 write by base address returns the byte count */
  trace((uint32_t)ioctl(IOCTL_FD_UART0, IOCTL_UART_WRITE, (void *)message,
    sizeof(message) - 1U));

  /* 2. seed by base address */
  trace((uint32_t)ioctl(IOCTL_FD_RNG, IOCTL_RNG_SET_SEED, UINT32_C(0x12345678)));

  /* 3. rejected output pointer must not advance the generator ... */
  trace((uint32_t)ioctl(IOCTL_FD_RNG, IOCTL_RNG_GET_DATA, KERNEL_ADDRESS));

  /* 4. ... so this is still the first value after the seed */
  trace((uint32_t)ioctl(IOCTL_FD_RNG, IOCTL_RNG_GET_DATA, &value));
  trace(value);

  /* 5. bad fd, bad op */
  trace((uint32_t)ioctl(99, 0));
  trace((uint32_t)ioctl(IOCTL_FD_RNG, 7, 0));

  /* 6. kernel source, ROM destination, negative length */
  trace((uint32_t)ioctl(IOCTL_FD_UART0, IOCTL_UART_WRITE, KERNEL_ADDRESS, 4));
  trace((uint32_t)ioctl(IOCTL_FD_UART0, IOCTL_UART_READ, SYSTEM_ROM_ADDRESS, 4));
  trace((uint32_t)ioctl(IOCTL_FD_UART0, IOCTL_UART_WRITE, (void *)message, -1));

  /* 7. unknown syscall number */
  trace((uint32_t)raw_syscall(99));

  /* 8. TRACE by base address */
  trace((uint32_t)ioctl(IOCTL_FD_TRACE, IOCTL_TRACE_EVENT, UINT32_C(0xabc)));

  __asm__ volatile("bkpt #0");
  while (1);
}
