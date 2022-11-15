// Inspired by https://github.com/capablevms/cheri-examples/blob/master/hybrid/compartment_examples/inter_comp_call/malicious_compartments/shared/switch_compartment.S

#define __ASSEMBLY__

#include "comp.h"

.section ".data.cheri_comp_manager"

.globl __comp_mgr_data_start
.hidden __comp_mgr_data_start
__comp_mgr_data_start:

.align 16
// An area reserved for the stack of the compartment manager (used for
// executing compartment swicthes).
COMP_MGR_STACK_BOTTOM:
    .fill COMP_MGR_TOTAL_STACK_SIZE - 16, 1, 0
COMP_MGR_STACK_TOP:
    .fill 16, 1, 0

COMP_CALL_ID:
    .long 0

// TODO: make sure `comp_switch` stays at the beginning of the .cheri_comp_manager
// section!!!
//
// It is essential that `comp_switch` stays at the beginning of the
// section because the compartment manager PCC is initialized to
// `__comp_mgr_code_start` whenver a compartment switch is initiated.
.section ".cheri_comp_manager", "ax", @progbits

// Read the value of COMP_CALL_ID into the specified register.
.macro comp_id to
    adr       \to, COMP_CALL_ID
    ldr       \to, [\to]
.endm

// Create a new manager stack frame for exectuing a compartment switch.
//
// This logically divides [COMP_MGR_STACK_BOTTOM, COMP_MGR_STACK_TOP] into
// frames (one frame for each compartment switch initiated using `comp_call`):
//
//                                                       _________________________
//  COMP_MGR_STACK_BOTTOM ----------------------------> |                         |
//                                                      |    comp_call frame 1    |
//                                                      |_________________________|
//  COMP_MGR_STACK_BOTTOM + COMP_MGR_FRAME_SIZE ------> |                         |
//                                                      |    comp_call frame 2    |
//                                                      |_________________________|
//                                                      |                         |
//                                                                ...
//                                                      |_________________________|
//                                                      |                         |
//                                                      |    comp_call frame N    |
//  COMP_MGR_STACK_TOP -------------------------------> |_________________________|
//
// This macro returns the top of the newly allocated frame in the specified
// register (`\to`).
.macro make_frame to
    // Read the value of `COMP_CALL_ID` into X9
    comp_id    x9
    // Load the manager stack
    adr        \to, COMP_MGR_STACK_BOTTOM
    // Use X9 to index through
    mov        x10, COMP_MGR_FRAME_SIZE
    mul        x9, x9, x10
    add        \to, \to, x9
.endm

.globl __comp_mgr_code_start
.hidden __comp_mgr_code_start
__comp_mgr_code_start:

// Switch to another compartment.
//
// Arguments:
//   C0  - target function (which might be in a different compartment)
//   C1  - source DDC
//   X2  - source SP
//   C29 - manager DDC
comp_switch:
    // The target DDC is stored in C29 (by a ldpblr instruction from the
    // compartment glue).
    msr        ddc, c29

    // Allocate a new stack frame.
    //
    // After allocating the new frame, the SP is updated accordingly (i.e. it is
    // initialized to the highest address of the frame):
    //                        ...
    //             |_________________________| <--- low
    //             |                         |
    //             |    comp_call frame M    |
    //  SP ------> |_________________________| <---- high
    make_frame x6
    mov        sp, x6

    // Store the source DDC and the old SP
    stp        c1, clr, [sp, #-48]!
    str        x2, [sp, #32]

    // Stack layout at this point:
    //
    //     `stack + size` -> ________________________
    //            sp + 40 -> [  <alignment pad>  ]   ^
    //            sp + 32 -> [ old SP            ]   |
    //            sp + 24 -> [ old CLR (hi64)    ]   |
    //            sp + 16 -> [ old CLR (lo64)    ]   | DDC bounds
    //            sp +  8 -> [ old DDC (high 64) ]   |
    //            sp +  0 -> [ old DDC (low 64)  ]   |
    //                                 :             :
    //            `stack` -> ________________________v

    // Enter the target compartment
    bl         comp_enter
    // Restore the source DDC and the SP
    ldp        c1, clr, [sp], #32
    ldr        x2, [sp], #16
    msr        ddc, c1
    mov        sp, x2
    ret        clr

// Increment the compartment count and return its previous value.
//
// NOTE: this equivalent to `return COMP_CALL_ID++;`.
//
// `COMP_CALL_ID` is used to index through `COMP_DATA_ADDR` and
// `COMP_MGR_STACK_BOTTOM`. A better solution would be to dynamically allocate
// (mmap) compartment data regions/compartment manager stack frames.
//
// XXX: This is never decremented, so bad things happen if more than
// `COMP_MAX_COMP_COUNT` calls are executed.
.globl next_comp_id
.hidden next_comp_id
next_comp_id:
   adr         x9, COMP_CALL_ID
   ldr         x0, [x9]
   add         x10, x0, 1
   str         x10, [x9]
   ret

// Enter the specified compartment
//
// Arguments:
//   C0 - the SP of the compartment
//   C1 - the PCC of the compartment
//   C2 - the DDC of the compartment
//   C3 - the address to jump to within the compartment
.globl enter_compartment
.hidden enter_compartment
enter_compartment:
    cvtp       clr, lr
    // Save the old DDC
    mrs        c9, DDC
    // Save the old SP
    mov        x10, sp
    // Install the new SP
    mov        sp, x0
    // Store the old CFP, CLR on the new stack
    stp        cfp, clr, [sp, #-64]!
    // Save the old DDC (C9) on the new stack
    str        c9, [sp, #32]
    // Save the old SP (X10) on the new stack
    str        x10, [sp, #48]

    // Stack layout at this point:
    //
    //     `stack + size` -> ________________________
    //            sp + 54 -> [  <alignment pad>  ]   ^
    //            sp + 48 -> [      old SP       ]   |
    //            sp + 40 -> [ old DDC (high 64) ]   |
    //            sp + 32 -> [ old DDC (low 64)  ]   |
    //            sp + 24 -> [ old CLR (hi64)    ]   |
    //            sp + 16 -> [ old CLR (lo64)    ]   | DDC bounds
    //            sp +  8 -> [ old CFP (high 64) ]   |
    //            sp +  0 -> [ old CFP (low 64)  ]   |
    //                                 :             :
    //            `stack` -> ________________________v

    // Install compartment DDC
    msr        DDC, c2
    // C3 contains the address to jump to
    mov        c0, c3
    // Jump into the compartment.
    //
    // NOTE: This instruction jumps to the address specified in the PCC, which
    // is not necessarily the appropriate place to enter the compartment. For
    // this reason, the code of every compartment is preceded by a trampoline,
    // which jumps to the desired entry point (specified in C0).
    blr        c1

    // Restore CFP CLR
    ldp        cfp, clr, [sp]
    // Restore DDC, stack
    ldr        c9, [sp, #32]
    ldr        x10, [sp, #48]
    msr        DDC, c9
    mov        sp, x10
    ret

// Dump the literal pool within the compartment boundary
.ltorg
