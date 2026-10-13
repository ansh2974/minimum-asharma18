#ifndef MINIMUM_IOCTL_H
#define MINIMUM_IOCTL_H

/* Syscall and ioctl numbers shared by the kernel and user programs.
 *
 * minemu/user_abi.h (supplied by the template, used by the tests in
 * tests/hw2) defines the syscall number, small file descriptors 0-4 and the
 * 0x1xx/0x2xx/0x3xx request codes. The assignment's ioctl table names the
 * devices by MMIO base address with per-device ops 0/1; those aliases are
 * defined here. The two sets never collide, so the kernel accepts both. */

#include "minemu/user_abi.h"

/* fd values from the assignment table */
#define IOCTL_FD_RNG 0x10003000
#define IOCTL_FD_UART0 0x10004000
#define IOCTL_FD_UART1 0x10005000
#define IOCTL_FD_TRACE 0x1000f000

/* op values from the assignment table */
#define IOCTL_RNG_SET_SEED 0x0  /* unsigned int seed */
#define IOCTL_RNG_GET_DATA 0x1  /* unsigned int *data */
#define IOCTL_UART_READ 0x0     /* char *dst, int n */
#define IOCTL_UART_WRITE 0x1    /* char *src, int n */
#define IOCTL_TRACE_EVENT 0x0   /* unsigned int value */

#endif
