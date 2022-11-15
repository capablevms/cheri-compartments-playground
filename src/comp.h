#pragma once

// The maximum supported number of compartments
#define COMP_MAX_COMP_COUNT        4
// The size of the compartment data area
#define COMP_DDC_SIZE              4096
// The total size of the area allocated for compartment data
#define COMP_TOTAL_DATA_SIZE       (COMP_MAX_COMP_COUNT * COMP_DDC_SIZE)
// The offset of the compartment stack in the data area
#define COMP_STACK_OFFSET          3072
// The maximum size allowed for any given compartment switch stack frame.
// NOTE: in practice, nothing prevents the managing compartment from creating
// frames that exceed the maxium frame size (however, doing so would corrupt the
// other active frames on the call stack).
#define COMP_MGR_FRAME_SIZE        4096
#define COMP_MGR_TOTAL_STACK_SIZE  (COMP_MAX_COMP_COUNT * COMP_MGR_FRAME_SIZE)

#ifndef __ASSEMBLY__

// Initiate a compartment switch from `SRC_COMP` to `TARGET_FN` (in another compartment).
#define comp_call(SRC_COMP, TARGET_FN)               \
    __asm__("adr x0, " #TARGET_FN "\n\t"             \
            "str clr, [sp, #-16]!\n\t"               \
            "bl __" #SRC_COMP "_switch_glue\n\t"     \
            "ldr clr, [sp], #16\n\t");

#define __comp(COMP_NAME) __attribute__((section(".cheri_comp_" COMP_NAME)))

// Initialize the library.
int comp_init(char *bin_path);

// Create a compartment with the specified entry point and jump to it.
int comp_enter(void (*comp_entry)());

#endif
