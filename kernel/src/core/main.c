#include "minemu/boot.h"
#include "minemu/irq.h"
#include "minemu/platform.h"
#include "minemu/trace.h"
#include "minemu/trap.h"
#include "minimum/loader.h"
#include "minimum/uart.h"

/* Name of the user module started at boot (see image/minimum.toml). */
#define INIT_MODULE_NAME "minimum-user"

#define TRACE_BAD_BOOT_INFO UINT32_C(0xb007bad0)
#define TRACE_BAD_MODULE UINT32_C(0xb007bad1)

void minemu_kernel_main(const struct minemu_boot_info *boot_info) {
    if ((uintptr_t)boot_info != MINEMU_BOOT_INFO_VADDR ||
        boot_info->magic != MINEMU_BOOT_INFO_MAGIC ||
        boot_info->version != MINEMU_ABI_VERSION ||
        boot_info->size != sizeof(*boot_info) ||
        boot_info->direct_map_vaddr != MINEMU_KERNEL_DIRECT_BASE ||
        boot_info->direct_map_paddr != MINEMU_RAM_BASE ||
        boot_info->direct_map_size != MINEMU_KERNEL_DIRECT_SIZE ||
        !loader_map_system_rom(boot_info)) {
        minemu_trace_event(TRACE_BAD_BOOT_INFO);
        minemu_fail_stop();
    }

    uint32_t entry;
    if (!loader_load_module(boot_info, INIT_MODULE_NAME, &entry)) {
        minemu_trace_event(TRACE_BAD_MODULE);
        minemu_fail_stop();
    }

    /* UART0 input is interrupt driven; IRQs are unmasked in USR mode by
     * minemu_enter_user, so only the device side is enabled here. */
    uart_init();

    minemu_enter_user(entry, USER_STACK_TOP);
}
