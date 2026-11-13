#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Compile production preference/layout logic with host-only SDL stubs.
# Linker discards device renderer code; no emulator or ARM64 packages needed.
set -euo pipefail
cd "$(dirname "$0")/.."
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cc -std=gnu11 -O1 -ffunction-sections -fdata-sections \
   -Wno-implicit-function-declaration -Wno-int-conversion \
   -Wno-incompatible-pointer-types -Itests/stubs -Isrc \
   tests/personalization-harness.c -Wl,--gc-sections -lm \
   -o "$work/preferences-test"
"$work/preferences-test"
