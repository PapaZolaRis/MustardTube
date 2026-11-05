#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
cc -std=c11 -O0 -ffunction-sections -fdata-sections -Wall -Wextra \
 $(pkg-config --cflags sdl2 SDL2_ttf SDL2_image) \
 tests/radio-ui-regression.c -Wl,--gc-sections -o build/radio-ui-regression
./build/radio-ui-regression
