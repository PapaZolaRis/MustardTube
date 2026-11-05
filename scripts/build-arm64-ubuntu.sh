#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
export PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=/
unset PKG_CONFIG_PATH || true
pkg-config --modversion sdl2 SDL2_ttf SDL2_image
cmake -S . -B build-arm64-ubuntu -G Ninja \
 -DCMAKE_SYSTEM_NAME=Linux \
 -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
 -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
 -DCMAKE_BUILD_TYPE=Release \
 -DCMAKE_C_FLAGS="-O2 -mcpu=cortex-a53"
cmake --build build-arm64-ubuntu --parallel 2
file build-arm64-ubuntu/mustardtube
