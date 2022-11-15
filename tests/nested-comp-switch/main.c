#include <comp.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Compartment baz
 */
__comp("baz") void
baz(void) {
    // todo: do something
}

/*
 * Compartment bar
 */
__comp("bar") void
bar(void) {
    comp_call(bar, baz);
}

__comp("bar") void
qux(void) {
    // todo: do something
}

/*
 * Compartment foo
 */
__comp("foo") void
foo(void) {
    // Compartment bar can be entered via any of its functions
    comp_call(foo, bar);
    comp_call(foo, qux);
}

__comp("foo") void
comp_entry(void) {
    foo();
}

int
main(int argc __attribute__((unused)), char **argv) {
    if (comp_init(argv[0])) {
        perror("failed to initialize compartmentalization lib");

        return EXIT_FAILURE;
    }

    // NOTE: `comp_entry` is not the only entry point of the foo compartment. We
    // could've just as well entered the compartment through `foo`.
    if (comp_enter(comp_entry)) {
        perror("failed to enter compartment");

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
