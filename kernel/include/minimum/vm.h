#ifndef MINIMUM_VM_H
#define MINIMUM_VM_H

#include <stdbool.h>
#include <stdint.h>

#include "minemu/mmu.h"

/* Physical address of the page directory installed by boot.S. */
#define VM_BOOTSTRAP_DIRECTORY_PADDR UINT32_C(0x40010000)

/* Kernel virtual address (direct map) of the active page directory. */
volatile uint32_t *vm_active_directory(void);

/* Translate a kernel direct-map pointer to its physical address. */
uint32_t vm_kernel_to_phys(const void *address);

/* Allocate one zeroed, page-aligned page of RAM above the kernel image and
 * return its kernel (direct-map) address, or NULL if RAM is exhausted.
 * Pages are never freed. Keeping page storage out of .bss matters because
 * the bootloader clears .bss one byte at a time. */
void *vm_alloc_page(void);

/* True if every page touched by [vaddr, vaddr + length) is mapped valid,
 * user-accessible and carries all of the PTE permission bits in required
 * (MINEMU_PTE_READABLE / MINEMU_PTE_WRITABLE / MINEMU_PTE_EXECUTABLE).
 * An empty range is always valid. */
bool vm_user_range_ok(uint32_t vaddr, uint32_t length, uint32_t required);

#endif
