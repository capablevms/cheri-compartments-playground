// Inspired by the [hybrid compartmentalization examples] from the CapableVMs
// [cheri-examples] repository.
//
// [hybrid compartmentalization examples]: https://github.com/capablevms/cheri-examples/blob/d6ecb72aaa479f6126a69b9e70de1ec2fc49cdc0/hybrid/compartment_examples/inter_comp_call/malicious_compartments/shared/switch_compartment.S
// [cheri-examples]: https://github.com/capablevms/cheri-examples/tree/d6ecb72aaa479f6126a69b9e70de1ec2fc49cdc0

#include "comp.h"

#include <assert.h>
#include <elf.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <cheriintrin.h>

// The size of the compartment data area
#define COMP_DDC_SIZE              4096
// The offset of the compartment stack in the data area
#define COMP_STACK_OFFSET          3072
#define MAX_SHNUM                  256

extern void enter_compartment(void *sp, void *__capability comp_pcc,
                              void *__capability comp_ddc, void (*comp_entry_fn)());

extern void comp_switch(void);

// A handle to the currently executing binary.
static FILE *ELF = NULL;
// The ELF header of the binary.
static Elf64_Ehdr ELF_HDR = {0};
// The cached seciton headers.
static Elf64_Shdr SHDRS[MAX_SHNUM] = {0};
// The sealed manager DDC and PCC.
static void *__capability SWITCHER_CAPS = NULL;

static inline  void
seal_cap(void *__capability cap) {
    __asm__("seal %w0, %w0, lpb" : "+r"(cap) :);
}

static inline void *__capability
read_ddc() {
    void *__capability ddc = NULL;
    __asm__("mrs %w0, DDC\n\t" : [ddc] "=C"(ddc) ::);
    return ddc;
}

void *
make_manager_stack(void) {
    return calloc(COMP_MGR_STACK_SIZE, 1);
}

// Parse the section headers of the specified binary.
static int
init_elf_info(char *bin_path) {
    if ((ELF = fopen(bin_path, "r")) == NULL) {
        perror("failed to open file");
        return EXIT_FAILURE;
    }

    if (fread(&ELF_HDR, sizeof(ELF_HDR), 1, ELF) != 1) {
        perror("failed to read ELF header");
        return EXIT_FAILURE;
    }

    if (ELF_HDR.e_shnum > MAX_SHNUM) {
        return EXIT_FAILURE;
    }

    if (fseek(ELF, ELF_HDR.e_shoff, SEEK_SET)) {
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < ELF_HDR.e_shnum; ++i) {
        if (fread(&SHDRS[i], sizeof(Elf64_Shdr), 1, ELF) != 1) {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

// Find the section `fn_addr` is located in.
//
// The section bounds are used to restrict the PCC of the compartment.
static void
find_section(void *fn_addr, Elf64_Addr *sh_addr, Elf64_Xword *sh_size) {
    uintptr_t addr = (uintptr_t)fn_addr;

    for (size_t i = 0; i < ELF_HDR.e_shnum; ++i) {
        uintptr_t section_start = SHDRS[i].sh_addr;
        uintptr_t section_end = SHDRS[i].sh_addr + SHDRS[i].sh_size;

        if (addr >= section_start && addr < section_end) {
            *sh_addr = SHDRS[i].sh_addr;
            *sh_size = SHDRS[i].sh_size;

            return;
        }
    }
}

static void *__capability
make_comp_ddc() {
    void *__capability ddc = (void *__capability)calloc(COMP_DDC_SIZE, 1);
    ddc = cheri_bounds_set(ddc, COMP_DDC_SIZE);

    return ddc;
}

static int
make_comp_caps(void *__capability *pcc, void *__capability *ddc, void (*fn_addr)()) {
    // Create compartment info
    Elf64_Addr sh_addr = 0;
    Elf64_Xword sh_size = 0;

    find_section((void *)fn_addr, &sh_addr, &sh_size);

    if (!sh_size) {
        return EXIT_FAILURE;
    }

    // Note: it's not possible to directly derive a valid capability from
    // sh_addr and sh_size (it has to be derived from fn_addr)
    *pcc = (void *__capability)fn_addr;
    *pcc = cheri_address_set(*pcc, sh_addr);
    *pcc = cheri_bounds_set(*pcc, sh_size);
    *ddc = make_comp_ddc();

    return EXIT_SUCCESS;
}

int
comp_init(char *bin_path) {
    // TODO: figure out the path of the executable using something like
    // /proc/self/exe (without requiring the caller to provide it)

    // Initialize `SHDRS` with the the section headers of the currently
    // executing binary. These are needed by `make_comp_caps` to determine the
    // PCC bounds when entering a compartment via `comp_enter` or `comp_call`.
    if (init_elf_info(bin_path)) {
        perror("failed to read ELF");
        return EXIT_FAILURE;
    }

    void *__capability *ddc_pcc = malloc(2 * sizeof(void *__capability));
    ddc_pcc[0] = read_ddc();
    ddc_pcc[1] = (void *__capability)comp_switch;
    // The compartment manager capabilities, which need to be copied into every compartment (they
    // are needed for compartment switching).
    SWITCHER_CAPS = (void *__capability)ddc_pcc;
    seal_cap(SWITCHER_CAPS);

    return EXIT_SUCCESS;
}

int
comp_enter(void (*comp_entry_fn)()) {
    void *__capability comp_pcc = NULL;
    void *__capability comp_ddc = NULL;

    // Set up the PCC and DDC for the compartment we're about to enter.
    if (make_comp_caps(&comp_pcc, &comp_ddc, comp_entry_fn)) {
        return EXIT_FAILURE;
    }

    void *comp_stack_top = (void *)((char *)__builtin_cheri_address_get(comp_ddc)  + COMP_STACK_OFFSET);

    // Copy the compartment manager capabilities into each compartment (to enable compartment
    // switching).
    void *__capability *comp_ddc_addr = (void *__capability *)__builtin_cheri_address_get(comp_ddc);
    *comp_ddc_addr = SWITCHER_CAPS;

    enter_compartment(comp_stack_top, comp_pcc, comp_ddc, comp_entry_fn);

    // TODO free or reuse compartment resources
    return EXIT_SUCCESS;
}
