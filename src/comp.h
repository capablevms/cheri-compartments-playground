#pragma once

// The size of the manager stack (which needs to be manually set up whenever a
// compartment switch is initiated).
#define COMP_MGR_STACK_SIZE        4096

#ifndef _ASM

#include <elf.h>
#include <stdio.h>

// Initiate a compartment switch from `SRC_COMP` to `TARGET_FN` (in another compartment).
#define comp_call(SRC_COMP, TARGET_FN)               \
    __asm__("adr x1, " #TARGET_FN "\n\t"             \
            "str clr, [sp, #-16]!\n\t"               \
            "bl __" #SRC_COMP "_switch_glue\n\t"     \
            "ldr clr, [sp], #16\n\t");

#define __comp(COMP_NAME) __attribute__((section(".cheri_comp_" COMP_NAME)))

struct elf_info {
    // A handle to the currently executing binary.
    FILE *elf;
    // The ELF header of the binary.
    Elf64_Ehdr elf_hdr;
    // The cached seciton headers.
    Elf64_Shdr *shdrs;
};

struct comp_ctx {
    // ELF strutctures necessary for identifying the PCC bounds of compartments.
    struct elf_info elf;
    // The sealed manager DDC and PCC.
    void *__capability switcher_caps;
};

/**
 * The compartment manager (opaque) context.
 */
typedef struct comp_ctx *comp_ctx_t;

/**
 * Initialize the library.
 */
int comp_init(char *bin_path, comp_ctx_t *);

/**
 * Create a compartment for the specified entry point function and jump to it.
 */
int comp_enter(comp_ctx_t ctx, void (*comp_entry)());

#endif
