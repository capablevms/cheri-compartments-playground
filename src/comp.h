#pragma once

// The size of the manager stack (which needs to be manually set up whenever a
// compartment switch is initiated).
#define COMP_MGR_STACK_SIZE        4096

#ifndef _ASM

// Initiate a compartment switch from `SRC_COMP` to `TARGET_FN` (in another compartment).
#define comp_call(SRC_COMP, TARGET_FN)               \
    __asm__("adr x0, " #TARGET_FN "\n\t"             \
            "str clr, [sp, #-16]!\n\t"               \
            "bl __" #SRC_COMP "_switch_glue\n\t"     \
            "ldr clr, [sp], #16\n\t");

#define __comp(COMP_NAME) __attribute__((section(".cheri_comp_" COMP_NAME)))

/**
 * Initialize the library.
 */
int comp_init(char *bin_path);

/**
 * Create a compartment for the specified entry point function and jump to it.
 */
int comp_enter(void (*comp_entry)());

#endif
