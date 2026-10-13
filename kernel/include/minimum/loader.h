#ifndef MINIMUM_LOADER_H
#define MINIMUM_LOADER_H

#include <stdbool.h>
#include <stdint.h>

#include "minemu/boot.h"

/* User address-space layout. */
#define USER_IMAGE_BASE UINT32_C(0x00400000)
#define USER_STACK_BOTTOM UINT32_C(0x007ff000)
#define USER_STACK_TOP UINT32_C(0x00800000)

/* Map the packaged image in System ROM into the kernel address space
 * (supervisor read-only). Returns false if boot_info describes a bad image. */
bool loader_map_system_rom(const struct minemu_boot_info *boot_info);

/* Find the module called name, copy its segments into user pages, map them
 * and a user stack, and store the entry address in *entry_vaddr. */
bool loader_load_module(const struct minemu_boot_info *boot_info, const char *name,
                        uint32_t *entry_vaddr);

/* Drop to A32 USR mode at entry_vaddr with the given user stack pointer
 * (implemented in user_entry.S). */
void minemu_enter_user(uint32_t entry_vaddr, uint32_t stack_top) __attribute__((noreturn));

#endif
