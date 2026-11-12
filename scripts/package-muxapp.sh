#!/usr/bin/env bash
# Historical packaging workflow. It cannot assemble a compliant Beta 2.2.8
# build with matching source and GPLv3 notices. Use ../build-package.sh.
if grep -Fq 'Version 3, 29 June 2007' "$(dirname "$0")/../LICENSE"; then
  echo 'Historical packaging flow disabled for GPLv3 builds. Run the parent build-package.sh.' >&2
  exit 2
fi
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT="$PWD"
BINARY="${MUSTARDTUBE_BINARY:-build-arm64-ubuntu/mustardtube}"
OUT='dist/MustardTube-v0.9.9-Andromeda-ARM64-TEST.muxapp'
for f in "$BINARY" tools/yt-dlp-onedir/yt-dlp tools/node tools/qjs tools/YT_DLP_THIRD_PARTY_LICENSES.txt tools/QUICKJS_LICENSE.txt tools/NODE_LICENSE.txt; do
  [ -s "$f" ] || { echo "Missing $f. Build the app and run bash scripts/build-runtime.sh first." >&2; exit 2; }
done
for bin in "$BINARY" tools/yt-dlp-onedir/yt-dlp tools/qjs tools/node; do
  file "$bin" | grep -Eq 'aarch64|ARM aarch64' || { echo "Not ARM64: $bin" >&2; exit 2; }
done
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
APP="$tmp/MustardTube"
mkdir -p "$APP/bin" "$APP/assets/themes" "$APP/assets/media" "$APP/scripts" "$APP/licenses" "$APP/glyph" "$APP/grid"
cp "$BINARY" "$APP/bin/mustardtube"
cp -a tools/yt-dlp-onedir/. "$APP/bin/"
cp tools/qjs tools/node "$APP/bin/"
cp assets/themes/*.theme "$APP/assets/themes/"
# Native muOS ICON header resolves glyph/mustardtube.png in application-list view.
cp assets/glyph/mustardtube.png "$APP/glyph/mustardtube.png"
cp assets/grid/mustardtube.png "$APP/grid/mustardtube.png"
cp assets/media/mustardtube-av-test.mp4 "$APP/assets/media/"
cp packaging/mux_launch.sh packaging/mux_lang.ini "$APP/"
cp scripts/youtube-backend.sh "$APP/scripts/"
cp tools/YT_DLP_THIRD_PARTY_LICENSES.txt tools/THIRD_PARTY_SOURCES.txt tools/QUICKJS_LICENSE.txt tools/NODE_LICENSE.txt LICENSE "$APP/licenses/"
cp docs/PAPAZOLARIS-USER-NOTICE.txt "$APP/licenses/USER_NOTICE.txt"
chmod 755 "$APP/bin/"* "$APP/mux_launch.sh" "$APP/scripts/youtube-backend.sh"
mkdir -p dist
rm -f "$OUT"
(cd "$tmp" && zip -q -r "$ROOT/$OUT" MustardTube)
echo "Created $OUT ($(du -h "$OUT" | cut -f1))"
echo 'v0.9.9 ARM64 Node + experimental 720p LIVE and VOD HLS A/V + unpackaged extractor must be validated on device before replacing the existing installation.'
