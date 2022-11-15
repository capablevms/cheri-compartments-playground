# cheri-compartments-playground

## Aim

This repo contains a proof-of-concept hybrid compartmentalization library for CheriBSD (for the
`morello-hybrid` target).

This library provides tooling for compartmentalizing existing C programs. It aims to support:
* execution compartmentalization: the PCC of each compartment is restricted according to the bounds
  established (statically) using the `__comp(COMP_NAME)` macro
* data compartmentalization: compartments have separate and non-overlapping data regions (TODO: this
  restriction will need to be relaxed to support data sharing between compartments)

The implementation is heavily inspired by [`inter_comp_call` hybrid compartmentalization examples]
from the CapableVMs [cheri-examples] repository. It doesn't add anything new other than the use of
sections to delineate compartment boundaries (for execution compartmentalization), and support for
compartments with multiple entry points.

## Usage

Entering and switching compartments can be achieved using:
* `comp_enter(dst_fn)`, which switches from "normal" (unrestricted) execution to compartmentalized
  (restricted) execution, jumping to the specified function
* `comp_call(src, dst_fn)`, a macro that initiates a switch from compartment `src` to the
  `dst_fn` function (which is executed in a separate compartment). `comp_call` transfers control to
  `comp_switch` (in the managing compartment), which creates a new compartment for `dst_fn` to
  execute in and jumps to the newly created compartment. Note: the fact that `src` must be specified
  by the `comp_call` caller is a limitation of the current implementation (in an ideal world, they
  would only need to specify the target function).

Note: The `__comp(COMP_NAME)` macro is meant to be used as a function attribute and is necessary for
execution compartmentalization. Each function must be associated with a compartment (identified by
`<COMP_NAME>`), and each compartment can comprise multiple functions (thus, compartments can have
multiple entry points). Under the hood, the macro places the annotated function in a
`.cheri_comp_<COMP_NAME>` section.

## Known limitations

* all the limitations of the the [`inter_comp_call` hybrid compartmentalization examples] from the
  CapableVMs [cheri-examples] repository (since this is heavily inspired by it)
* `comp_init` requires the caller to supply the name of the executable
* `comp_call` requires the caller to supply the name of the calling compartment (`src`)
* the glue code required for compartment switching/entering is not auto-generated (the user of the
  library has to make sure it is present)
* the existing code is very fragile: it expects the compartment trampolines to precede the
  compartment code (however, nothing actually enforces this ordering). A potential solution would be
  to use a linker script to make sure the trampolines code stays put
* the managing compartment doesn't implement any sort of memory management (the memory region
  designated as the compartment data area is used as a stack). In other words, compartmentalized
  programs can only use the stack (i.e. they cannot access global variables, the heap, or anything
  else outside the bounds of the memory area allocated by the compartment manager). As a
  consequence, compartmentalized programs can't call many of the functions from the standard
  library (as they depend on state located outside the compartment bounds)

## Build instructions

### Prerequisites

Build and run CheriBSD for the `morello-hybrid` target using [`cheribuild`].

### Running the tests

The tests can be found in [`test.bats`](./test.bats). You can run them using:

```
SSHPORT=<your QEMU ssh port> make test
```

If you want to build and run a specific test program, use one of the `run-<test name>` targets:
```
SSHPORT=<your QEMU ssh port> make run-<test name>
```

where `<test name>` is the name one of the subdirectories of `tests`.

The resulting binaries are generated in `bin/`:

```
bin
├── libcomp.so
└── tests
    ├── nested-comp-switch
    ├── out-of-bounds-call
    └── simple-comp-switch
```

[`inter_comp_call` hybrid compartmentalization examples]: https://github.com/capablevms/cheri-examples/blob/d6ecb72aaa479f6126a69b9e70de1ec2fc49cdc0/hybrid/compartment_examples/inter_comp_call/malicious_compartments/shared/switch_compartment.S
[cheri-examples]: https://github.com/capablevms/cheri-examples/tree/d6ecb72aaa479f6126a69b9e70de1ec2fc49cdc0
[`cheribuild`]: https://github.com/CTSRD-CHERI/cheribuild
