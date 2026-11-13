#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Beta 2.4.5 visual consistency on Beta 2.4.4 UI polish and Beta 2.4.3 personalization and validated Beta 2.4.2 OTA foundation with four-theme UI, controlled package rebuild. Requires hardware-validated Beta 2.2.7 installer
# only for unchanged third-party runtimes and first-party radio helpers.
set -euo pipefail
if (( $# != 1 )); then
    echo 'Usage: bash build-package.sh /path/to/MustardTube-v0.9.11-Beta2.2.7-HD-Streaming.muxapp' >&2
    exit 2
fi
input=$(realpath "$1")
cd "$(dirname "$0")"
root=$PWD
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
unzip -q "$input" -d "$stage"
app="$stage/MustardTube"
[[ -d "$app" ]] || { echo 'Invalid baseline installer' >&2; exit 2; }
(cd "$app" && sha256sum -c "$root/baseline.sha256")
cd "$root"
bash scripts/test-youtube-backend.sh
bash scripts/test-display-compat.sh
bash scripts/test-beta22-source.sh
bash scripts/test-beta226a-ui.sh
python3 scripts/test-beta230-layout.py
python3 scripts/test-beta231-ui.py
python3 scripts/test-beta232-themes.py
node --check scripts/ota-update.js
node scripts/test-ota-update.js
python3 scripts/test-beta242-ota-ui.py
python3 scripts/test-beta243-personalization.py
python3 scripts/test-beta244-ui-polish.py
python3 scripts/test-beta245-visual-states.py
bash scripts/test-beta243-personalization.sh
python3 scripts/test-beta241-ui.py
python3 scripts/test-beta240-ui.py
bash scripts/build-arm64-ubuntu.sh
bash scripts/build-radio-hotkeys-arm64.sh
file tools/radio-hotkeys | grep -Eq 'aarch64|ARM aarch64'
file build-arm64-ubuntu/mustardtube | grep -Eq 'aarch64|ARM aarch64'
# Preserve validated runtime dependencies; rebuild first-party GUI including
# its GPL About screen (never reuse the prior MIT-branded GUI executable).
cp build-arm64-ubuntu/mustardtube "$app/bin/mustardtube"
cp tools/radio-hotkeys "$app/bin/radio-hotkeys"
cp scripts/youtube-backend.sh scripts/radio-backend.js scripts/background-radio.js scripts/ota-update.js "$app/scripts/"
cp packaging/mux_launch.sh "$app/mux_launch.sh"
cp assets/glyph/mustardtube.png "$app/glyph/mustardtube.png"
cp assets/grid/mustardtube.png "$app/grid/mustardtube.png"
# Preserve every theme; in older builds Paper/Forest remained stale because
# only Midnight was copied. Header branding uses separate colour PNGs.
mkdir -p "$app/assets/themes" "$app/assets/branding"
cp assets/themes/*.theme "$app/assets/themes/"
cp assets/branding/header-dark.png assets/branding/header-paper.png "$app/assets/branding/"
mkdir -p "$app/licenses" "$app/source"
cp LICENSE "$app/licenses/LICENSE"
cp NOTICE "$app/licenses/NOTICE.txt"
cp docs/HISTORICAL-MIT-LICENSE.txt "$app/licenses/HISTORICAL-MIT-LICENSE.txt"
cp docs/PAPAZOLARIS-USER-NOTICE.txt "$app/licenses/USER_NOTICE.txt"
cp docs/SOURCE-ACCESS.txt "$app/licenses/SOURCE-ACCESS.txt"
cp docs/THIRD-PARTY-LICENSE-REVIEW.md "$app/licenses/THIRD-PARTY-LICENSE-REVIEW.md"
# Also ship attribution that the original package placed in the assets dir.
cp assets/flags/ATTRIBUTION.txt "$app/licenses/TWEMOJI_ATTRIBUTION.txt"
chmod 755 "$app/mux_launch.sh" "$app/bin/mustardtube" "$app/bin/radio-hotkeys" "$app/scripts/youtube-backend.sh"
chmod 644 "$app/scripts/background-radio.js" "$app/scripts/radio-backend.js" "$app/scripts/ota-update.js"
# Supply exact first-party source/build scripts together with compiled installer.
# Complete Corresponding Source of bundled GPL/LGPL third-party components must
# be audited separately; this step does NOT certify those obligations.
version=$(awk -F'"' '/^#define MT_VERSION / {print $2}' src/release.h)
beta=$(awk -F'"' '/^#define MT_BETA / {print $2}' src/release.h)
src_name="MustardTube-v${version}-Beta${beta}-Source.zip"
(cd "$root" && zip -qr "$stage/$src_name" \
    src scripts assets docs packaging tests tools \
    .gitignore BRANDING.md CMakeLists.txt CONTRIBUTING.md LICENSE NOTICE README.md \
    build-package.sh baseline.sha256 \
    -x 'build/*' 'build-*/*' 'dist/*' '.git/*' \
       '*/__pycache__/*' '*.pyc' \
       'tools/yt-dlp-onedir/*' \
       'tools/qjs' 'tools/node' 'tools/ffplay-radio' 'tools/radio-hotkeys')
unzip -tq "$stage/$src_name" >/dev/null
cp "$stage/$src_name" "$app/source/$src_name"
mkdir -p "$root/dist"
cp "$stage/$src_name" "$root/dist/$src_name"
out="$root/dist/MustardTube-v${version}-Beta${beta}-OTA.muxapp"
(cd "$stage" && zip -qr "$out" MustardTube)
unzip -tq "$out" >/dev/null
# Catch real packaging omissions that static source-only tests missed.
for item in \
  "glyph/mustardtube.png:assets/glyph/mustardtube.png" \
  "grid/mustardtube.png:assets/grid/mustardtube.png" \
  "assets/themes/midnight.theme:assets/themes/midnight.theme" \
  "assets/themes/forest.theme:assets/themes/forest.theme" \
  "assets/themes/paper.theme:assets/themes/paper.theme" \
  "assets/themes/acidbloom.theme:assets/themes/acidbloom.theme" \
  "assets/branding/header-dark.png:assets/branding/header-dark.png" \
  "assets/branding/header-paper.png:assets/branding/header-paper.png" \
  "scripts/ota-update.js:scripts/ota-update.js"; do
    installed=${item%%:*}; source=${item#*:}
    cmp -s "$source" <(unzip -p "$out" "MustardTube/$installed") || {
      echo "Packaged branding mismatch: $installed" >&2; exit 1;
    }
done
for binary in bin/ffplay-radio bin/node bin/yt-dlp bin/qjs; do
  if unzip -Z1 "$input" | grep -Fxq "MustardTube/$binary"; then
    cmp -s <(unzip -p "$input" "MustardTube/$binary") \
       <(unzip -p "$out" "MustardTube/$binary") || {
      echo "Unexpected change to baseline dependency: $binary" >&2; exit 1;
    }
  fi
done
(cd "$root/dist" && sha256sum "$(basename "$out")" "$src_name" > SHA256SUMS.txt)
sha256sum "$out" "$root/dist/$src_name" | tee "$out.sha256"
echo "Prepared new candidate: $out"
echo 'WARNING: Not verified on RG35XX devices; do not publish before ownership and bundled-source audit.'
