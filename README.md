# cheri-compartments-playground

## Aim

This repo contains a proof-of-concept hybrid compartmentalization library for CheriBSD (for the
`morello-hybrid` target).

The aim of the library is to provide tooling for compartmentalizing existing C programs with respect
to code and data.

The library aims to support:
* execution compartmentalization: the PCC of each compartment is restricted according to the bounds
  established (statically) using the `__comp(COMP_NAME)` macro
* data compartmentalization: compartments have separate and non-overlapping data regions (TODO: this
  restriction will need to be relaxed to support data sharing between compartments)

The implementation is heavily inspired by [`inter_comp_call` hybrid compartmentalization examples]
from the CapableVMs [cheri-examples] repository.

## Usage

Entering and switching compartments can be achieved using:
* `comp_enter(dst_fn)`, which switches from "normal" execution to compartmentalized execution,
  jumping to the specified function
* `comp_call(src, dst_fn)`, a macro that initiates a switch from the `src` compartment to the
  `dst_fn` function (which is executed in a separate compartment). Note: the fact that `src` must be
  specified by the `comp_call` caller is a limitation of the current implementation (in an ideal
  world, they would only need to specify the target function)

Note: The `__comp(COMP_NAME)` macro is meant to be used as a function attribute and is necessary for
execution compartmentalization. Each function must be associated with a compartment (identified by
`<COMP_NAME>`), and each compartment can comprise multiple functions (thus, compartments can have
multiple entry points). Under the hood, the macro places the annotated function in a
`.cheri_comp_<COMP_NAME>` section.

## Known limitations

* all the limitations of the the [`inter_comp_call` hybrid compartmentalization examples] from the
  CapableVMs [cheri-examples] repository (since this is heavily inspired by it)
* the "glue" code required for compartment switching/entering is not auto-generated (the user of the
  library has to make sure it is present)
* the existing code is very fragile: it expects the compartment trampolines to precede the
  compartment code (however, nothing actually enforces this ordering). A potential solution would be
  to use a linker script to make sure the trampolines stay put
* lack of automated tests
* the managing compartment doesn't implement any sort of memory management (the memory region
  designated as the compartment data area is used as a stack)
* the library heavily relies on global variables
* `dladdr` and `dl_iterate_phdr` are non-standard (although in practice, I think they're both
  implemented on all the platforms we target)

[`inter_comp_call` hybrid compartmentalization examples]: https://github.com/capablevms/cheri-examples/blob/d6ecb72aaa479f6126a69b9e70de1ec2fc49cdc0/hybrid/compartment_examples/inter_comp_call/malicious_compartments/shared/switch_compartment.S
[cheri-examples]: https://github.com/capablevms/cheri-examples/tree/d6ecb72aaa479f6126a69b9e70de1ec2fc49cdc0

## Build instructions

### Prerequisites

Build and run CheriBSD for the `morello-hybrid` target using [`cheribuild`].

### Running the example

To build and run the example:

```
SSHPORT=<your QEMU ssh port> make  run
```

The resulting `example` and `libcomp.so` binaries are generated in
`bin/morello-hybrid/`.

[`cheribuild`]: https://github.com/CTSRD-CHERI/cheribuild
