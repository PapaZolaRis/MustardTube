#!/usr/bin/env bash
# Optional app-owned, event-driven in-game control; stock kernel Linux evdev only.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p tools
CC="${CC:-aarch64-linux-gnu-gcc}"
"$CC" -std=c11 -Os -mcpu=cortex-a53 -Wall -Wextra -Wpedantic -D_FORTIFY_SOURCE=2 \
  -fstack-protector-strong src/radio_hotkeys.c src/game_mixer.c -o tools/radio-hotkeys
file tools/radio-hotkeys
readelf -h tools/radio-hotkeys | grep -E 'Class:|Machine:'
