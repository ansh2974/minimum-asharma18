#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "minemu/mmu.h"
#include "minemu/platform.h"
#include "minimum/vm.h"

static bool phys_in_ram(uint32_t paddr) {
    return paddr >= MINEMU_RAM_BASE && paddr - MINEMU_RAM_BASE < MINEMU_RAM_SIZE;
}

static uintptr_t phys_to_kernel(uint32_t paddr) {
    return (uintptr_t)(paddr - MINEMU_RAM_BASE + MINEMU_KERNEL_DIRECT_BASE);
}

volatile uint32_t *vm_active_directory(void) {
    return (volatile uint32_t *)phys_to_kernel(VM_BOOTSTRAP_DIRECTORY_PADDR);
}

uint32_t vm_kernel_to_phys(const void *address) {
    return (uint32_t)(uintptr_t)address - MINEMU_KERNEL_DIRECT_BASE + MINEMU_RAM_BASE;
}

/* defined in kernel.ld, page aligned */
extern char __minemu_kernel_end[];

void *vm_alloc_page(void) {
    static uintptr_t next_free;

    if (next_free == 0) {
        next_free = (uintptr_t)__minemu_kernel_end;
    }
    if (next_free - MINEMU_KERNEL_DIRECT_BASE > MINEMU_KERNEL_DIRECT_SIZE - MINEMU_PAGE_SIZE) {
        return NULL;
    }

    uint32_t *page = (uint32_t *)next_free;
    next_free += MINEMU_PAGE_SIZE;
    for (uint32_t i = 0; i < MINEMU_PAGE_SIZE / 4U; ++i) {
        page[i] = 0;
    }
    return page;
}

/* Look up the PTE for one page; returns 0 (not valid) if unmapped. */
static uint32_t lookup_pte(uint32_t vaddr) {
    uint32_t pde = vm_active_directory()[vaddr >> 22];
    if ((pde & MINEMU_PDE_VALID) == 0) {
        return 0;
    }
    uint32_t table_paddr = pde & MINEMU_PDE_PAGE_MASK;
    if (!phys_in_ram(table_paddr)) {
        return 0;
    }
    const volatile uint32_t *table = (const volatile uint32_t *)phys_to_kernel(table_paddr);
    return table[(vaddr >> 12) & 1023U];
}

bool vm_user_range_ok(uint32_t vaddr, uint32_t length, uint32_t required) {
    if (length == 0) {
        return true;
    }
    uint32_t last = vaddr + (length - 1U);
    if (last < vaddr) {
        /* wraps around the address space */
        return false;
    }

    uint32_t need = MINEMU_PTE_VALID | MINEMU_PTE_USER | required;
    uint32_t last_page = last & MINEMU_PTE_PAGE_MASK;
    for (uint32_t page = vaddr & MINEMU_PTE_PAGE_MASK;; page += MINEMU_PAGE_SIZE) {
        if ((lookup_pte(page) & need) != need) {
            return false;
        }
        if (page == last_page) {
            return true;
        }
    }
}
