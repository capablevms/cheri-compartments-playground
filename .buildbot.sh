#!/bin/bash
#
# Copyright (c) 2020-2022 The CapableVMs Contributors.
# SPDX-License-Identifier: MIT OR Apache-2.0

set -eou pipefail

su -l buildbot

echo "Checking that the tests pass..."

export SSHPORT=10022

args=(
    --architecture morello-hybrid
    # Qemu System to use
    --qemu-cmd $CHERI_DIR/sdk/bin/qemu-system-morello
    --bios edk2-aarch64-code.fd
    --disk-image $CHERI_DIR/cheribsd-morello-hybrid.img
    # Required build-dir in CheriBSD
    --build-dir .
    --ssh-port $SSHPORT
    --ssh-key $HOME/.ssh/id_ed25519.pub
)

BUILDBOT_PLATFORM=morello-hybrid python3 .buildbot-run-tests.py "${args[@]}"
