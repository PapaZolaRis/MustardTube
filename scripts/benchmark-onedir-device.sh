#!/usr/bin/env bash
# Non-invasive benchmark of official yt-dlp unpackaged ARM64 build.
# Usage: bash scripts/benchmark-onedir-device.sh root@192.168.0.32
set -euo pipefail
cd "$(dirname "$0")/.."
HOST="${1:-root@192.168.0.32}"
[ -s tools/yt-dlp-onedir/yt-dlp ] || { echo 'Run build-runtime.sh first' >&2;exit 2; }
REMOTE='/mnt/mmc/ARCHIVE/.mustardtube-runtime-probe'
TAR="$(mktemp --suffix=.tar.gz)";trap 'rm -f "$TAR"' EXIT
# Archive relative layout so PyInstaller's _internal directory stays beside ELF.
tar -czf "$TAR" -C tools/yt-dlp-onedir .
echo 'Transferring unpackaged runtime to the SD card for a one-time launch test.'
ssh "$HOST" "mkdir -p '$REMOTE'"
scp "$TAR" "$HOST:$REMOTE/runtime.tar.gz"
ssh "$HOST" "tar -xzf '$REMOTE/runtime.tar.gz' -C '$REMOTE' && rm -f '$REMOTE/runtime.tar.gz'"
ssh "$HOST" "sh -s" <<'REMOTE_SCRIPT'
ROOT='/mnt/mmc/ARCHIVE/.mustardtube-runtime-probe'
APP='/run/muos/storage/application/MustardTube'
for item in "$APP/bin/yt-dlp" "$ROOT/yt-dlp"; do
  echo "=== $(basename $(dirname "$item")) / yt-dlp --version ==="
  T0=$(date +%s)
  RC=0
  V=$(timeout 30 "$item" --version 2>&1) || RC=$?
  echo "duration=$(($(date +%s)-T0))s exit=$RC version=$V"
done
REMOTE_SCRIPT
echo "Temporary runtime remains at $REMOTE to permit diagnostics."
echo "To remove it: ssh '$HOST' 'rm -rf '$REMOTE'"
