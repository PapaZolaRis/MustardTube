#!/usr/bin/env bash
# Developer-only. Builds self-contained ARM64 yt-dlp + Node.js (QuickJS fallback).
# No Python or PortMaster installation is required on the handheld.
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT="$PWD"
mkdir -p tools dist
YTDLP_VER=2026.10.26
ZIP_NAME=yt-dlp_linux_aarch64.zip
BASE="https://github.com/yt-dlp/yt-dlp/releases/download/$YTDLP_VER"
SUMS_URL="$BASE/SHA2-256SUMS"
NODE_VER=24.22.2
NODE_ARCHIVE="node-v${NODE_VER}-linux-arm64.tar.xz"
NODE_BASE="https://nodejs.org/dist/v${NODE_VER}"
QJS_VER=2026-06-04
QJS_URL="https://bellard.org/quickjs/quickjs-${QJS_VER}.tar.xz"
command -v aarch64-linux-gnu-gcc >/dev/null || { echo 'ARM64 GCC missing' >&2; exit 2; }
for dep in curl unzip file sha256sum tar; do
  command -v "$dep" >/dev/null || { echo "Required command missing: $dep" >&2; exit 2; }
done
curl -fL --retry 3 "$SUMS_URL" -o tools/SHA2-256SUMS
expected="$(awk '$2=="yt-dlp_linux_aarch64.zip" || $2=="*yt-dlp_linux_aarch64.zip" {print $1}' tools/SHA2-256SUMS)"
[ -n "$expected" ] || { echo 'No ARM64 unpackaged archive checksum in release' >&2; exit 2; }
if [ ! -s "tools/$ZIP_NAME" ]; then
  echo 'Downloading the official unpackaged ARM64 yt-dlp runtime...'
  curl -fL --retry 3 "$BASE/$ZIP_NAME" -o "tools/$ZIP_NAME"
fi
printf '%s  %s\n' "$expected" "tools/$ZIP_NAME" | sha256sum -c -
if [ ! -s tools/yt-dlp-onedir/yt-dlp ]; then
  scratch="$(mktemp -d)"
  trap 'rm -rf "$scratch"' EXIT
  unzip -q "tools/$ZIP_NAME" -d "$scratch"
  exe=''
  # Accept the upstream release's root-folder name without assuming it.
  while IFS= read -r candidate; do
    if file "$candidate" | grep -Eqi 'aarch64|ARM aarch64'; then
      exe="$candidate"
      break
    fi
  done < <(find "$scratch" -maxdepth 4 -type f \( -name 'yt-dlp' -o -name 'yt-dlp_linux_aarch64' \) -print)
  [ -n "$exe" ] || { echo 'Cannot locate ARM64 yt-dlp executable inside official ZIP' >&2; exit 2; }
  mkdir -p tools/yt-dlp-onedir
  cp -a "$(dirname "$exe")"/. tools/yt-dlp-onedir/
  exe_name="$(basename "$exe")"
  if [ "$exe_name" != yt-dlp ]; then
    mv "tools/yt-dlp-onedir/$exe_name" tools/yt-dlp-onedir/yt-dlp
  fi
fi
chmod 755 tools/yt-dlp-onedir/yt-dlp
file tools/yt-dlp-onedir/yt-dlp | grep -Eqi 'aarch64|ARM aarch64' || {
  echo 'yt-dlp unpackaged executable is not ARM64' >&2; exit 2;
}
# Use the exact Node version proven functional on RG35XX Plus.
# Verify the official release archive even when reusing the benchmark download.
curl -fsSL --retry 3 "$NODE_BASE/SHASUMS256.txt" -o tools/NODE_SHASUMS256.txt
node_expected="$(awk -v name="$NODE_ARCHIVE" '$2==name || $2=="*"name {print $1}' tools/NODE_SHASUMS256.txt)"
[ -n "$node_expected" ] || { echo 'Official Node SHA manifest missing Linux ARM64 tarball' >&2;exit 2; }
if [ ! -s "tools/$NODE_ARCHIVE" ]; then
  cached="$HOME/projects/.mustardtube-node-benchmark/$NODE_ARCHIVE"
  if [ -s "$cached" ]; then
    echo 'Reusing Node archive from successful device benchmark...'
    cp "$cached" "tools/$NODE_ARCHIVE"
  else
    echo 'Downloading official Node.js Linux ARM64...'
    curl -fL --retry 3 "$NODE_BASE/$NODE_ARCHIVE" -o "tools/$NODE_ARCHIVE.partial"
    mv "tools/$NODE_ARCHIVE.partial" "tools/$NODE_ARCHIVE"
  fi
fi
printf '%s  %s\n' "$node_expected" "tools/$NODE_ARCHIVE" | sha256sum -c -
node_root="node-v${NODE_VER}-linux-arm64"
tar -xJOf "tools/$NODE_ARCHIVE" "$node_root/bin/node" > tools/node.tmp
mv -f tools/node.tmp tools/node
chmod 755 tools/node
file tools/node | grep -Eqi 'aarch64|ARM aarch64' || { echo 'Node binary is not ARM64' >&2;exit 2; }
tar -xJOf "tools/$NODE_ARCHIVE" "$node_root/LICENSE" > tools/NODE_LICENSE.txt
[ -s tools/NODE_LICENSE.txt ] || { echo 'Node distribution licence missing' >&2;exit 2; }
if [ ! -f tools/qjs ]; then
  echo 'Cross-building QuickJS runtime for ARM64...'
  if [ -z "${scratch:-}" ]; then
    scratch="$(mktemp -d)"
    trap 'rm -rf "$scratch"' EXIT
  fi
  temp_qjs="$scratch/qjs-work"
  mkdir -p "$temp_qjs"
  curl -fL --retry 3 "$QJS_URL" -o "$temp_qjs/quickjs.tar.xz"
  tar -xJf "$temp_qjs/quickjs.tar.xz" -C "$temp_qjs"
  make -C "$temp_qjs/quickjs-$QJS_VER" -j2 CROSS_PREFIX=aarch64-linux-gnu- qjs
  cp "$temp_qjs/quickjs-$QJS_VER/qjs" tools/qjs
  sed -n '1,22p' "$temp_qjs/quickjs-$QJS_VER/Makefile" > tools/QUICKJS_LICENSE.txt
fi
[ -s tools/QUICKJS_LICENSE.txt ] || { echo 'QuickJS license missing. Rebuild qjs + license.' >&2;exit 2; }
chmod 755 tools/qjs
file tools/qjs | grep -Eqi 'aarch64|ARM aarch64' || { echo 'qjs is not ARM64' >&2;exit 2; }
curl -fsSL --retry 2 "https://raw.githubusercontent.com/yt-dlp/yt-dlp/$YTDLP_VER/THIRD_PARTY_LICENSES.txt" \
  -o tools/YT_DLP_THIRD_PARTY_LICENSES.txt || {
    echo 'Cannot fetch third-party license notices; packaging should not proceed.' >&2;exit 2;
  }
cat > tools/THIRD_PARTY_SOURCES.txt <<EOF
yt-dlp $YTDLP_VER (official unpackaged Linux aarch64, SHA256 verified)
$BASE/$ZIP_NAME
Node.js $NODE_VER (official linux arm64; distribution licence bundled)
$NODE_BASE/$NODE_ARCHIVE
QuickJS $QJS_VER (fallback; MIT license)
$QJS_URL
Neither project is affiliated with MustardTube.
EOF
echo "ARM64 onedir yt-dlp ready: $(du -sh tools/yt-dlp-onedir | cut -f1); Node: $(du -h tools/node | cut -f1)"
