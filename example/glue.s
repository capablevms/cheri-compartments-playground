// TODO these should be auto-generated/copied into every compartment
.macro trampoline
    str       clr, [sp, #-16]!
    blr       c0
    ldr       clr, [sp], #16
    ret       clr
.endm

.macro switch_glue
    str       clr, [sp, #-16]!
    mrs       c9, ddc
    // *DDC contains the address of the sealed switcher capability
    ldr       c9, [x9]
    mrs       c1, ddc
    mov       x2, sp
    // C9 contains the address of the sealed switcher capability
    ldpblr    c29, [c9]
    // Restore the clr
    ldr       clr, [sp], #16
    ret
.endm

// TODO make sure the trampolines stay at the beginning of the section
.section .cheri_comp_foo

// void __foo_trampoline(void *__capability target_addr);
.globl __foo_trampoline
__foo_trampoline:
    trampoline

.globl __foo_switch_glue
__foo_switch_glue:
    switch_glue

.section .cheri_comp_bar

// void __bar_trampoline(void *__capability target_addr);
.globl __bar_trampoline
__bar_trampoline:
    trampoline

.globl __bar_switch_glue
__bar_switch_glue:
    switch_glue

.section .cheri_comp_baz

// void __baz_trampoline(void *__capability target_addr);
.globl __baz_trampoline
__baz_trampoline:
    trampoline

.globl __baz_switch_glue
__baz_switch_glue:
    switch_glue
