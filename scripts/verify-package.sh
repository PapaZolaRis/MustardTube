#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
PKG="${1:-dist/MustardTube-v0.9.9-Andromeda-ARM64-TEST.muxapp}"
[ -s "$PKG" ] || { echo "Package missing: $PKG" >&2; exit 2; }
unzip -t "$PKG" | tail -1
names="$(unzip -Z1 "$PKG")"
for item in MustardTube/mux_launch.sh MustardTube/bin/mustardtube \
  MustardTube/bin/yt-dlp MustardTube/bin/node MustardTube/bin/qjs \
  MustardTube/scripts/youtube-backend.sh \
  MustardTube/licenses/QUICKJS_LICENSE.txt MustardTube/licenses/NODE_LICENSE.txt \
  MustardTube/glyph/mustardtube.png \
  MustardTube/grid/mustardtube.png \
  MustardTube/assets/themes/midnight.theme \
  MustardTube/assets/themes/forest.theme \
  MustardTube/assets/themes/paper.theme; do
    grep -Fxq "$item" <<< "$names" || { echo "Missing: $item" >&2; exit 1; }
done
if grep -Eq '^application/|^/|(^|/)\.\.(/|$)' <<< "$names"; then
    echo 'Unsafe muxapp archive structure' >&2; exit 1
fi
# Onedir runtime must ship internal data, not only the launcher ELF.
if ! grep -Eq '^MustardTube/bin/_internal/[^/]+|^MustardTube/bin/lib/[^/]+' <<< "$names"; then
    echo 'Missing unpackaged yt-dlp support directory in muxapp' >&2
    exit 1
fi
echo 'Archive includes unpackaged ARM64 yt-dlp + verified Node.js with licences. Device validation required.' 
