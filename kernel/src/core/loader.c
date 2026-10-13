#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "minemu/boot.h"
#include "minemu/mmu.h"
#include "minemu/platform.h"
#include "minimum/loader.h"
#include "minimum/vm.h"

/* Adapted from examples/user-mode-hello/kernel/main.c. Page tables and user
 * pages come from vm_alloc_page instead of static arrays, so they do not
 * inflate the kernel's .bss (which the bootloader clears byte by byte). */

#define MAX_USER_IMAGE_PAGES 16U
#define SYSTEM_ROM_TABLE_COUNT 4U

/* backing page for each user image page, indexed in allocation order */
static uint8_t *user_pages[MAX_USER_IMAGE_PAGES];
static uint32_t user_page_vaddrs[MAX_USER_IMAGE_PAGES];
static uint32_t user_page_flags[MAX_USER_IMAGE_PAGES];
static size_t user_page_count;

static void copy_bytes(void *destination, const void *source, size_t length) {
    uint8_t *out = destination;
    const uint8_t *in = source;
    for (size_t i = 0; i < length; ++i) {
        out[i] = in[i];
    }
}

static bool bytes_equal(const uint8_t *left, const char *right, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        if (left[i] != (uint8_t)right[i]) {
            return false;
        }
    }
    return true;
}

static size_t string_length(const char *s) {
    size_t n = 0;
    while (s[n] != '\0') {
        ++n;
    }
    return n;
}

/* Bounds checks against the image, written to avoid overflow. */
static bool image_range_valid(uint32_t offset, uint32_t length, uint32_t image_size) {
    return offset <= image_size && length <= image_size - offset;
}

static bool image_records_valid(uint32_t offset, uint32_t count, uint32_t image_size) {
    return offset <= image_size && count <= ((image_size - offset) >> 5);
}

bool loader_map_system_rom(const struct minemu_boot_info *boot_info) {
    if (boot_info->system_rom_base != MINEMU_SYSTEM_ROM_BASE || boot_info->image_size == 0 ||
        boot_info->image_size > MINEMU_SYSTEM_ROM_SIZE) {
        return false;
    }

    uint32_t page_count = (boot_info->image_size + MINEMU_PAGE_SIZE - 1U) / MINEMU_PAGE_SIZE;
    uint32_t table_count = (page_count + 1023U) / 1024U;
    if (table_count > SYSTEM_ROM_TABLE_COUNT) {
        return false;
    }

    volatile uint32_t *directory = vm_active_directory();
    for (uint32_t table = 0; table < table_count; ++table) {
        uint32_t *entries = vm_alloc_page();
        if (entries == NULL) {
            return false;
        }
        uint32_t remaining = page_count - table * 1024U;
        uint32_t count = remaining < 1024U ? remaining : 1024U;
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t page = table * 1024U + i;
            /* supervisor-only, read-only */
            entries[i] = (boot_info->system_rom_base + page * MINEMU_PAGE_SIZE) |
                         MINEMU_PTE_VALID | MINEMU_PTE_READABLE;
        }
        directory[(boot_info->system_rom_base >> 22) + table] =
            vm_kernel_to_phys(entries) | MINEMU_PDE_VALID;
    }
    minemu_mmu_tlbiall();
    return true;
}

static const struct minemu_module_record *find_module(const struct minemu_boot_info *boot_info,
                                                       const char *name) {
    const uint8_t *image = (const uint8_t *)(uintptr_t)boot_info->system_rom_base;
    size_t name_length = string_length(name);
    if (!image_records_valid(boot_info->module_table_offset, boot_info->module_count,
                             boot_info->image_size)) {
        return NULL;
    }

    const struct minemu_module_record *modules =
        (const struct minemu_module_record *)(image + boot_info->module_table_offset);
    for (uint32_t i = 0; i < boot_info->module_count; ++i) {
        const struct minemu_module_record *module = &modules[i];
        if (module->name_length == name_length &&
            image_range_valid(module->name_offset, module->name_length, boot_info->image_size) &&
            bytes_equal(image + module->name_offset, name, name_length)) {
            return module;
        }
    }
    return NULL;
}

static uint32_t pte_flags_from_segment(uint32_t segment_flags) {
    uint32_t pte_flags = 0;
    if ((segment_flags & MINEMU_SEGMENT_READABLE) != 0) {
        pte_flags |= MINEMU_PTE_READABLE;
    }
    if ((segment_flags & MINEMU_SEGMENT_WRITABLE) != 0) {
        pte_flags |= MINEMU_PTE_WRITABLE;
    }
    if ((segment_flags & MINEMU_SEGMENT_EXECUTABLE) != 0) {
        pte_flags |= MINEMU_PTE_EXECUTABLE;
    }
    return pte_flags;
}

/* Find (or, if create is set, allocate) the backing page for page_vaddr.
 * Two segments sharing a page must agree on its permissions. */
static int user_page_index(uint32_t page_vaddr, uint32_t flags, bool create) {
    for (size_t i = 0; i < user_page_count; ++i) {
        if (user_page_vaddrs[i] == page_vaddr) {
            if (create && user_page_flags[i] != flags) {
                return -1;
            }
            return (int)i;
        }
    }
    if (!create || user_page_count == MAX_USER_IMAGE_PAGES) {
        return -1;
    }
    uint8_t *page = vm_alloc_page();
    if (page == NULL) {
        return -1;
    }
    size_t i = user_page_count++;
    user_pages[i] = page;
    user_page_vaddrs[i] = page_vaddr;
    user_page_flags[i] = flags;
    return (int)i;
}

static bool prepare_user_range(uint32_t vaddr, uint32_t length, uint32_t flags) {
    uint32_t end = vaddr + length;
    for (uint32_t page = vaddr & MINEMU_PTE_PAGE_MASK; page < end; page += MINEMU_PAGE_SIZE) {
        if (user_page_index(page, flags, true) < 0) {
            return false;
        }
    }
    return true;
}

/* Copy length bytes from source into the backing pages of
 * [vaddr, vaddr + length). The pages start out zeroed, so whatever part of a
 * segment is not copied (its BSS) is already zero. */
static bool write_user_range(uint32_t vaddr, const uint8_t *source, uint32_t length) {
    while (length != 0) {
        uint32_t page_vaddr = vaddr & MINEMU_PTE_PAGE_MASK;
        int i = user_page_index(page_vaddr, 0, false);
        if (i < 0) {
            return false;
        }
        uint32_t offset = vaddr - page_vaddr;
        uint32_t chunk = MINEMU_PAGE_SIZE - offset;
        if (chunk > length) {
            chunk = length;
        }
        copy_bytes(user_pages[i] + offset, source, chunk);
        source += chunk;
        vaddr += chunk;
        length -= chunk;
    }
    return true;
}

static bool segment_valid(const struct minemu_module_segment *segment, uint32_t image_size) {
    const uint32_t known = MINEMU_SEGMENT_READABLE | MINEMU_SEGMENT_WRITABLE |
                           MINEMU_SEGMENT_EXECUTABLE;
    return segment->memory_size != 0 && segment->file_size <= segment->memory_size &&
           (segment->flags & ~known) == 0 &&
           /* executable implies readable */
           ((segment->flags & MINEMU_SEGMENT_EXECUTABLE) == 0 ||
            (segment->flags & MINEMU_SEGMENT_READABLE) != 0) &&
           /* must fit between the image base and the stack */
           segment->virtual_address >= USER_IMAGE_BASE &&
           segment->virtual_address < USER_STACK_BOTTOM &&
           segment->memory_size <= USER_STACK_BOTTOM - segment->virtual_address &&
           image_range_valid(segment->data_offset, segment->file_size, image_size);
}

bool loader_load_module(const struct minemu_boot_info *boot_info, const char *name,
                        uint32_t *entry_vaddr) {
    const struct minemu_module_record *module = find_module(boot_info, name);
    if (module == NULL) {
        return false;
    }

    const uint8_t *image = (const uint8_t *)(uintptr_t)boot_info->system_rom_base;
    if (module->segment_count == 0 ||
        !image_records_valid(module->segment_table_offset, module->segment_count,
                             boot_info->image_size) ||
        (module->entry_vaddr & 3U) != 0) {
        return false;
    }

    const struct minemu_module_segment *segments =
        (const struct minemu_module_segment *)(image + module->segment_table_offset);
    bool entry_is_executable = false;
    user_page_count = 0;

    for (uint32_t i = 0; i < module->segment_count; ++i) {
        const struct minemu_module_segment *segment = &segments[i];
        if (!segment_valid(segment, boot_info->image_size)) {
            return false;
        }

        uint32_t pte_flags = pte_flags_from_segment(segment->flags);
        if (!prepare_user_range(segment->virtual_address, segment->memory_size, pte_flags) ||
            !write_user_range(segment->virtual_address, image + segment->data_offset,
                              segment->file_size)) {
            return false;
        }

        if ((segment->flags & MINEMU_SEGMENT_EXECUTABLE) != 0 && segment->file_size >= 4U &&
            module->entry_vaddr >= segment->virtual_address &&
            module->entry_vaddr - segment->virtual_address <= segment->file_size - 4U) {
            entry_is_executable = true;
        }
    }
    if (!entry_is_executable) {
        return false;
    }

    uint32_t *page_table = vm_alloc_page();
    uint8_t *stack = vm_alloc_page();
    if (page_table == NULL || stack == NULL) {
        return false;
    }
    for (size_t i = 0; i < user_page_count; ++i) {
        page_table[(user_page_vaddrs[i] >> 12) & 1023U] =
            vm_kernel_to_phys(user_pages[i]) | MINEMU_PTE_VALID | MINEMU_PTE_USER |
            user_page_flags[i];
    }
    /* writable, non-executable stack */
    page_table[(USER_STACK_BOTTOM >> 12) & 1023U] =
        vm_kernel_to_phys(stack) | MINEMU_PTE_VALID | MINEMU_PTE_USER | MINEMU_PTE_READABLE |
        MINEMU_PTE_WRITABLE;

    vm_active_directory()[USER_IMAGE_BASE >> 22] =
        vm_kernel_to_phys(page_table) | MINEMU_PDE_VALID;
    minemu_mmu_tlbiall();

    *entry_vaddr = module->entry_vaddr;
    return true;
}
