#!/usr/bin/env bash
# Static compatibility preflight only; requires actual device audio testing.
set -euo pipefail
player="${1:-tools/ffplay-radio}"
[[ -s "$player" ]] || { echo "Player file missing: $player" >&2; exit 2; }
command -v readelf >/dev/null || { echo 'readelf missing' >&2; exit 2; }
e_machine=$(readelf -h "$player" | sed -n 's/^ *Machine: *//p')
case "$e_machine" in *AArch64*) ;; *) echo "Wrong architecture: $e_machine" >&2;exit 2;; esac
needed=$(readelf -d "$player" | sed -n 's/.*(NEEDED).*\[\([^]]*\)\].*/\1/p')
printf 'ARM64 player: %s\nDynamic dependencies (must all exist on stock Andromeda):\n%s\n' "$player" "${needed:-none reported}"
if grep -Eq '^lib(avcodec|avformat|avutil|avfilter|avdevice|swresample|swscale|postproc)\.so' <<< "$needed";then
  echo 'FAIL: unbundled dynamic FFmpeg libraries; build static FFmpeg libraries or bundle dependencies with correct loader paths' >&2
  exit 1
fi
max_ver=$(readelf -W --version-info "$player" 2>/dev/null | grep -Eo 'GLIBC_[0-9]+\.[0-9]+' | sed 's/GLIBC_//' | sort -V | tail -n 1 || true)
if [[ -n "$max_ver" ]];then
  echo "Highest referenced GLIBC symbol: $max_ver (reported handheld baseline: 2.44)"
  if [[ $(printf '%s\n' 2.44 "$max_ver" | sort -V | tail -n 1) != '2.44' ]];then
    echo 'FAIL: built binary references newer GLIBC than recorded handheld target' >&2
    exit 1
  fi
fi
echo 'PASS static architecture and FFmpeg linkage checks; still requires runtime dependency and device audio tests.'
