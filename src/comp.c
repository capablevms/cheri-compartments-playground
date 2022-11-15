// Inspired by the [hybrid compartmentalization examples] from the CapableVMs
// [cheri-examples] repository.
//
// [hybrid compartmentalization examples]: https://github.com/capablevms/cheri-examples/blob/d6ecb72aaa479f6126a69b9e70de1ec2fc49cdc0/hybrid/compartment_examples/inter_comp_call/malicious_compartments/shared/switch_compartment.S
// [cheri-examples]: https://github.com/capablevms/cheri-examples/tree/d6ecb72aaa479f6126a69b9e70de1ec2fc49cdc0

#include "comp.h"

#include <elf.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <link.h>
#include <dlfcn.h>
#include <assert.h>

#include <cheriintrin.h>

#define __comp_mgr_data __attribute__((section(".data.cheri_comp_manager")))
#define __comp_mgr_code __attribute__((section(".cheri_comp_manager")))
#define MAX_SHNUM 256

extern void __comp_mgr_code enter_compartment(void *sp, void *__capability comp_pcc,
        void *__capability comp_ddc, void *__capability comp_entry);

extern void __comp_mgr_code __comp_mgr_code_start(void);

extern size_t __comp_mgr_code next_comp_id(void);
extern void __comp_mgr_data_start(void);

// A handle to the currently executing binary.
static __comp_mgr_data FILE *ELF = NULL;
// The ELF header of the binary.
static __comp_mgr_data Elf64_Ehdr ELF_HDR = {0};
// The cached seciton headers.
static __comp_mgr_data Elf64_Shdr SHDRS[MAX_SHNUM] = {0};
// The manager DDC and PCC.
static __comp_mgr_data void *__capability DDC_PCC[2];
// The sealed manager DDC and PCC.
static __comp_mgr_data void *__capability SWITCHER_CAPS = NULL;
// The manager PCC.
static __comp_mgr_data void *__capability MGR_PCC = NULL;
// The manager DDC.
static __comp_mgr_data void *__capability MGR_DDC = NULL;

// Compartment data area.
// NOTE: This is currently only used for the compartment stack.
static
__attribute__((aligned(16))) char COMP_DATA[COMP_TOTAL_DATA_SIZE] = {0};

// The address of `COMP_DATA`.
//
// This address must be stored within the boundaries of the compartment manager
// data section during initialization, before restricting the DDC (this is
// necessary because the address of `COMP_DATA` is loaded from a literal pool
// located outside the managing compartment boundary).
//
// TODO: come up with a more readable workaround.
static __comp_mgr_data void *__capability COMP_DATA_ADDR = NULL;

static inline  __comp_mgr_code void
seal_cap(void *__capability cap) {
    __asm__("seal %w0, %w0, lpb" : "+r"(cap) :);
}

// Parse the section headers of the specified binary.
static __comp_mgr_code int
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

    for (size_t i = 0; i < ELF_HDR.e_shnum; ++i) {
        if (fseek(ELF, ELF_HDR.e_shoff + i * sizeof(Elf64_Shdr), SEEK_SET)) {
            return EXIT_FAILURE;
        }

        if (fread(&SHDRS[i], sizeof(Elf64_Shdr), 1, ELF) != 1) {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

// Find the section `fn_addr` is located in.
//
// The section bounds are used to restrict the PCC of the compartment.
static __comp_mgr_code void
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
make_comp_ddc(size_t comp_id) {
    // `COMP_DATA_ADDR` is statically allocated region logically divided into
    // `COMP_DDC_SIZE` chunks (each chunk represents the data region of a
    // compartment).
    void *__capability ddc = COMP_DATA_ADDR;
    size_t comp_offset = comp_id * COMP_DDC_SIZE;
    uintptr_t ddc_addr = (uintptr_t)((char *)cheri_address_get(ddc) + comp_offset);

    ddc = cheri_address_set(ddc, ddc_addr);
    ddc = cheri_bounds_set(ddc, COMP_DDC_SIZE);

    return ddc;
}

static __comp_mgr_code int
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

    // TODO: dynamically allocate the compartment data area.
    *ddc = make_comp_ddc(next_comp_id());

    return EXIT_SUCCESS;
}

static __comp_mgr_code int
find_mgr_ddc(struct dl_phdr_info *info, size_t size __attribute__((unused)), void *data __attribute__((unused))) {
    for (int i = 0; i < info->dlpi_phnum; ++i) {
        uintptr_t addr  = info->dlpi_addr + info->dlpi_phdr[i].p_vaddr;

        if ((uintptr_t)&__comp_mgr_data_start >= addr
                && (uintptr_t)&__comp_mgr_data_start < addr + info-> dlpi_phdr[i].p_memsz) {
            MGR_DDC = cheri_address_set(MGR_DDC, addr);
            // TODO: are the bounds correct?
            MGR_DDC = cheri_bounds_set(MGR_DDC, info->dlpi_phdr[i].p_memsz);
        }

        if (MGR_DDC) {
            // This tells dl_iterate_phdr to stop iterating.
            return 1;
        }
    }

    return 0;
}

// Initialize and restrict the DDC and PCC
static __comp_mgr_code int
find_comp_mgr_bounds() {
    MGR_PCC = (void *__capability)__comp_mgr_code_start;
    // TODO: restrict the PCC of the managing compartment?

    Dl_info dl_info;

    // Find out where we are loaded
    if (!dladdr((void *)&__comp_mgr_data_start, &dl_info)) {
        return EXIT_FAILURE;
    }

    MGR_DDC = (void *__capability)dl_info.dli_fbase;

    // Figure out the bounds of the segment we're in
    dl_iterate_phdr(find_mgr_ddc, NULL);

    if (MGR_PCC == NULL || MGR_DDC == NULL) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

__comp_mgr_code int
comp_init(char *bin_path) {
    // Initialize `SHDRS` with the the section headers of the currently
    // executing binary. These are needed by `make_comp_caps` to determine the
    // PCC bounds when entering a compartment via `comp_enter` or `comp_call`.
    if (init_elf_info(bin_path)) {
        perror("failed to read ELF");
        return EXIT_FAILURE;
    }

    // Prepare the compartment manager (by initializing `MGR_DDC` and `MGR_PCC`).
    if (find_comp_mgr_bounds()) {
        return EXIT_FAILURE;
    }

    // To understand why this is needed, see the comment above `COMP_DATA_ADDR`.
    COMP_DATA_ADDR = &COMP_DATA;
    DDC_PCC[0] = MGR_DDC;
    DDC_PCC[1] = MGR_PCC;

    SWITCHER_CAPS = (void *__capability)DDC_PCC;

    seal_cap(SWITCHER_CAPS);

    return EXIT_SUCCESS;
}

__comp_mgr_code int
comp_enter(void (*comp_entry_fn)()) {
    void *__capability comp_pcc = NULL;
    void *__capability comp_ddc = NULL;

    // Set up the PCC and DDC for the compartment we're about to enter.
    if (make_comp_caps(&comp_pcc, &comp_ddc, comp_entry_fn)) {
        return EXIT_FAILURE;
    }

    void *comp_stack_top = (void *)((char *)__builtin_cheri_address_get(comp_ddc)  + COMP_STACK_OFFSET);

    // Copy the manager caps into each compartment (to enable compartment switching).
    void *__capability *comp_ddc_addr = (void *__capability *)__builtin_cheri_address_get(comp_ddc);
    *comp_ddc_addr = SWITCHER_CAPS;

    enter_compartment(comp_stack_top, comp_pcc, comp_ddc, (void *__capability)comp_entry_fn);

    // TODO free or reuse compartment resources
    return EXIT_SUCCESS;
}
