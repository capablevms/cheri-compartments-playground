#!/usr/bin/env bats

# Run the specified example.
#
# $1 - the name of the example to run
run_example() (
    ssh -o 'StrictHostKeyChecking no' -p "$SSHPORT" "$RUNUSER@$RUNHOST" -t "cd $RUNDIR/cheri-comp && LD_LIBRARY_PATH=./bin/ ./bin/tests/$1"
)

@test "simple inter-compartment calls succeed" {
    run run_example simple-comp-switch
    [ "${status}" -eq 0 ]
}

@test "nested inter-compartment calls succeed" {
    run run_example nested-comp-switch
    [ "${status}" -eq 0 ]
}

@test "out-of-compartment-bounds calls trigger SIGPROT" {
    run run_example out-of-bounds-call
    [ "${status}" -ne 0 ]
}
