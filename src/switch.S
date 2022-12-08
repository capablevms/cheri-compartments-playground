// Inspired by https://github.com/capablevms/cheri-examples/blob/master/hybrid/compartment_examples/inter_comp_call/malicious_compartments/shared/switch_compartment.S

#define _ASM

#include "comp.h"

#define COMP_MGR_TMP_STACK_SIZE 1024

.data
.align 16
// An area which serves as provisional stack for the compartment manager.
//
// When a compartment switch is initiated, the stack of the caller is replaced
// with the provisional stack. Once the provisional stack is installed, we call
// `make_manager_stack` to allocate the "real" stack of the manager.
COMP_MGR_TMP_STACK:
    .fill COMP_MGR_TMP_STACK_SIZE - 16, 1, 0
COMP_MGR_TMP_STACK_TOP:
    .fill 16, 1, 0

.text

.macro save_regs
    stp	c0, c1, [sp, #-32]!
    stp	c2, c3, [sp, #-32]!
    stp	c4, c5, [sp, #-32]!
    stp	c6, c7, [sp, #-32]!
    stp	c8, c9, [sp, #-32]!
    stp	c10, c11, [sp, #-32]!
    stp	c12, c13, [sp, #-32]!
    stp	c14, c15, [sp, #-32]!
    stp	c16, c17, [sp, #-32]!
    stp	c29, c30, [sp, #-32]!
.endm

.macro load_regs
    ldp	c29, c30, [sp], #32
    ldp	c16, c17, [sp], #32
    ldp	c14, c15, [sp], #32
    ldp	c12, c13, [sp], #32
    ldp	c10, c11, [sp], #32
    ldp	c8, c9, [sp], #32
    ldp	c6, c7, [sp], #32
    ldp	c4, c5, [sp], #32
    ldp	c2, c3, [sp], #32
    ldp	c0, c1, [sp], #32
.endm

// Switch to another compartment.
//
// `comp_switch` is reached by calling `comp_call` from a compartment (which
// executes an `ldpblr` instruction to branch to this code)
//
// Arguments:
//   X0  - the context (comp_ctx_t *)
//   X1  - target function (which might be in a different compartment)
//   C2  - source DDC
//   C29 - manager DDC
.globl comp_switch
.hidden comp_switch
comp_switch:
    // The target DDC is stored in C29 (by a ldpblr instruction from the
    // compartment glue).
    msr        ddc, c29
    // Install a temporary stack to call `make_manager_stack` (which creates the
    // stack frame for the compartment switch).
    adr        x9, COMP_MGR_TMP_STACK_TOP
    mov        x10, sp
    mov        sp, x9
    // Save the caller-saved registers (which could be clobbered by `make_manager_stack`)
    save_regs
    bl         make_manager_stack
    mov        x19, x0
    // Restore the caller-saved registers
    load_regs
    // Use the stack returned by `make_manager_stack`
    mov        sp, x19

    // Store the source DDC and the old SP on the newly created stack
    stp        c2, clr, [sp, #-48]!
    str        x10, [sp, #32]

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
    ldp        c2, clr, [sp], #32
    ldr        x10, [sp], #16
    msr        ddc, c2
    mov        sp, x10
    ret        clr

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
    // which jumps to the desired entry point (specified in X0).
    blr        c1

    // Restore CFP, CLR
    ldp        cfp, clr, [sp]
    // Restore DDC, stack
    ldr        c9, [sp, #32]
    ldr        x10, [sp, #48]
    msr        DDC, c9
    mov        sp, x10
    ret
