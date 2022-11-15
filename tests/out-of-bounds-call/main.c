#include <comp.h>
#include <stdio.h>
#include <stdlib.h>

__comp("bar") void bar(void);

/*
 * Compartment foo
 */
__comp("foo") void
foo(void) {
    bar();
}

/*
 * Compartment bar
 */
__comp("bar") void
bar(void) {
    // Unreachable, because calling bar from foo will trigger a SIGPROT.
}

int
main(int argc __attribute__((unused)), char **argv) {
    if (comp_init(argv[0])) {
        perror("failed to initialize compartmentalization lib");

        return EXIT_FAILURE;
    }

    if (comp_enter(foo)) {
        perror("failed to enter compartment");

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
