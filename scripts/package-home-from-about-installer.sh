#!/usr/bin/env bash
# Repack the already working PapaZolaRis v0.9.9 About build with only
# the new UI executable and its additive home-discovery backend changes.
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT="$PWD"
BASE="${1:-}"
if [[ ! -f "$BASE" ]]; then
  echo 'Usage: bash scripts/package-home-from-about-installer.sh /absolute/path/to/PapaZolaRis-About-v0.9.9.muxapp' >&2
  exit 2
fi
BASE="$(readlink -f "$BASE")"
NEW_BIN="$ROOT/build-arm64-ubuntu/mustardtube"
[[ -s "$NEW_BIN" ]] || { echo 'Run scripts/build-arm64-ubuntu.sh first.' >&2;exit 2; }
file "$NEW_BIN" | grep -q 'ARM aarch64' || { echo 'Expected aarch64 executable' >&2;exit 2; }
unzip -tq "$BASE" >/dev/null
TMP="$(mktemp -d)";trap 'rm -rf "$TMP"' EXIT
unzip -q "$BASE" -d "$TMP"
[[ -f "$TMP/MustardTube/scripts/youtube-backend.sh" ]] || { echo 'Unexpected base installer' >&2;exit 2; }
cp "$NEW_BIN" "$TMP/MustardTube/bin/mustardtube"
cp "$ROOT/scripts/youtube-backend.sh" "$TMP/MustardTube/scripts/youtube-backend.sh"
chmod 755 "$TMP/MustardTube/bin/mustardtube" "$TMP/MustardTube/scripts/youtube-backend.sh"
mkdir -p "$ROOT/dist"
OUT="$ROOT/dist/MustardTube-v0.9.10-Home-Explore-PapaZolaRis-Andromeda-ARM64-RC1.muxapp"
(cd "$TMP" && zip -qr "$OUT" MustardTube)
unzip -tq "$OUT" >/dev/null
for part in mux_launch.sh licenses/USER_NOTICE.txt; do
 cmp -s <(unzip -p "$BASE" "MustardTube/$part") <(unzip -p "$OUT" "MustardTube/$part") || {
   echo "Unexpected change in $part" >&2;exit 1;
 }
done
printf 'Packaged %s\n' "$OUT"
