#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
cc -std=c11 -O2 -Wall -Wextra -Wpedantic \
 $(pkg-config --cflags sdl2 SDL2_ttf SDL2_image) \
 tests/home-ui-regression.c \
 $(pkg-config --libs sdl2 SDL2_ttf SDL2_image) -lm \
 -o build/home-ui-regression
./build/home-ui-regression
