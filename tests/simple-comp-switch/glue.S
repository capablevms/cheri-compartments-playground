#include <asmacros.h>

// TODO make sure the trampolines stay at the *beginning* of each section
.section .cheri_comp_foo

// void __foo_trampoline(void *__capability target_addr);
.globl __foo_trampoline
__foo_trampoline:
    TRAMPOLINE

.section .cheri_comp_bar

// void __bar_trampoline(void *__capability target_addr);
.globl __bar_trampoline
__bar_trampoline:
    TRAMPOLINE

.section .cheri_comp_foo

.globl __foo_switch_glue
__foo_switch_glue:
    SWITCH_GLUE

.section .cheri_comp_bar

.globl __bar_switch_glue
__bar_switch_glue:
    SWITCH_GLUE
