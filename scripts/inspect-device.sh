#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 1 ]]; then
  echo 'Usage: bash scripts/inspect-device.sh root@HANDHELD_IP' >&2
  exit 2
fi
TARGET="$1"
# Read-only checks. No changes are made to the handheld.
ssh -o ConnectTimeout=10 "$TARGET" 'sh -s' <<'REMOTE'
echo '=== Architecture / firmware ==='
uname -m
if [ -f /etc/os-release ]; then grep -E '^(NAME|VERSION|PRETTY_NAME)=' /etc/os-release; fi
if command -v getconf >/dev/null 2>&1; then getconf GNU_LIBC_VERSION 2>/dev/null || true; fi
printf '\n=== App lifecycle function ===\n'
if [ -r /opt/muos/script/var/func.sh ]; then echo 'func.sh: present'; else echo 'func.sh: missing'; fi
printf '\n=== SDL libraries (paths only) ===\n'
for root in /usr/lib /usr/local/lib /lib /opt/muos/lib; do
  if [ -d "$root" ]; then
    find "$root" -maxdepth 3 -type f \( -name '*SDL2*.so*' -o -name '*freetype*.so*' \) -print 2>/dev/null
    find "$root" -maxdepth 3 -type l \( -name '*SDL2*.so*' -o -name '*freetype*.so*' \) -print 2>/dev/null
  fi
done
printf '\n=== Available fonts (names only) ===\n'
for root in /usr/share/fonts /opt/muos/share/font /opt/muos/share/fonts; do
  if [ -d "$root" ]; then find "$root" -maxdepth 4 -type f \( -iname '*.ttf' -o -iname '*.otf' \) -print 2>/dev/null | head -30; fi
done
printf '\n=== Runtime application mount ===\n'
if [ -d /run/muos/storage/application ]; then echo 'Application mount: present'; else echo 'Application mount: absent'; fi
printf '\n=== Runtime linker ===\n'
for loader in /lib/ld-linux-aarch64.so.1 /lib64/ld-linux-aarch64.so.1; do
  if [ -e "$loader" ]; then echo "$loader"; fi
done
REMOTE
