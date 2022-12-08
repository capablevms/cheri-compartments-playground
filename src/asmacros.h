#pragma once

// TODO these should be auto-generated/copied into every compartment
#define SWITCH_GLUE                                                    \
    str       clr, [sp, #-16]!                                        ;\
    mrs       c9, ddc                                                 ;\
    /* *(DDC + 16) contains a pointer to the comp_ctx_t            */ ;\
    ldr       x0, [x9, #16]                                           ;\
    /* *DDC contains the address of the sealed switcher capability */ ;\
    ldr       c9, [x9]                                                ;\
    mrs       c2, ddc                                                 ;\
    /* C9 contains the address of the sealed switcher capability */   ;\
    ldpblr    c29, [c9]                                               ;\
    /* Restore the clr */                                             ;\
    ldr       clr, [sp], #16                                          ;\
    ret

#define TRAMPOLINE                                                     \
    str       clr, [sp, #-16]!                                        ;\
    blr       x0                                                      ;\
    ldr       clr, [sp], #16                                          ;\
    ret       clr
