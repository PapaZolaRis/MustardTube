#!/usr/bin/env bash
# Developer-only: build an audio-only ffplay for MustardTube's background mode.
# Usage: bash scripts/build-background-player-arm64.sh /path/to/ffmpeg-source
# Must run in the prepared Ubuntu cross-compilation VM with ARM64 SDL2 and
# OpenSSL development headers/libs. Never run on an end-user handheld.
set -euo pipefail
cd "$(dirname "$0")/.."
root="$PWD"
src="${1:-}"
[[ -n "$src" && -x "$src/configure" ]] || {
  echo 'Usage: build-background-player-arm64.sh /absolute/path/to/ffmpeg-source' >&2
  exit 2
}
src=$(realpath "$src")
for cmd in aarch64-linux-gnu-gcc pkg-config make file readelf;do
  command -v "$cmd" >/dev/null || { echo "Missing developer tool: $cmd" >&2;exit 2; }
done
export PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=/
unset PKG_CONFIG_PATH || true
pkg-config --exists sdl2 openssl || { echo 'ARM64 sdl2/openssl pkg-config modules required in the build VM' >&2;exit 2; }
mkdir -p build/ffplay-radio-arm64 tools
cd build/ffplay-radio-arm64
"$src/configure" \
  --target-os=linux --arch=aarch64 --cross-prefix=aarch64-linux-gnu- \
  --enable-cross-compile --cpu=cortex-a53 \
  --pkg-config=pkg-config --disable-doc --disable-debug --disable-programs \
  --enable-ffplay --disable-everything --enable-network \
  --enable-openssl --enable-sdl2 \
  --enable-decoder=mp3,mp3float,aac,aac_fixed,flac,vorbis,opus,pcm_s16le \
  --enable-demuxer=mp3,aac,flac,ogg,hls,mpegts,mov,concat \
  --enable-parser=mpegaudio,aac,vorbis,opus \
  --enable-protocol=file,http,https,tcp,tls,crypto,httpproxy,udp \
  --enable-bsf=aac_adtstoasc \
  --enable-filter=abuffer,abuffersink,aresample,aformat,volume \
  --extra-cflags=-mcpu=cortex-a53
make -j2 ffplay
cp ffplay "$root/tools/ffplay-radio"
file "$root/tools/ffplay-radio" | grep -Eq 'aarch64|ARM aarch64' || { echo 'Not an ARM64 player' >&2;exit 2; }
chmod 755 "$root/tools/ffplay-radio"
cp "$src/COPYING.LGPLv2.1" "$root/tools/FFPLAY_LGPL_LICENSE.txt"
cp "$src/COPYING.GPLv2" "$root/tools/FFPLAY_GPL_LICENSE.txt"
printf 'FFmpeg source used for bundled audio player: %s\n' "$(basename "$src")" > "$root/tools/FFPLAY_SOURCE.txt"
readelf -d "$root/tools/ffplay-radio" | grep NEEDED || true
printf '\nPlayer built. IMPORTANT: check linked libraries on the RG35XX Plus\n'
printf 'and verify audio-only/no-framebuffer operation + PipeWire mixing.\n'
