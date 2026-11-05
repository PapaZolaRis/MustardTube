#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
shim="${MUSTARDTUBE_SDL_SHIM:-}"
[[ -n "$shim" && -f "$shim/SDL.h" ]] || { echo 'Set MUSTARDTUBE_SDL_SHIM to the build-only SDL ABI header folder';exit 2; }
cc -std=c11 -O2 -ffunction-sections -fdata-sections -I"$shim" tests/combined-ui-regression.c \
  -Wl,--gc-sections -l:libSDL2-2.0.so.0 -lm -o /tmp/mustardtube-combined-ui-test
/tmp/mustardtube-combined-ui-test
