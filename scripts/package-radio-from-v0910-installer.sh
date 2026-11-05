#!/usr/bin/env bash
# Packages v0.9.11 Combined Beta 2 from an existing known-good v0.9.10 runtime.
# Run on the prepared Ubuntu ARM64 cross-compile VM after build-arm64-ubuntu.sh.
set -euo pipefail
cd "$(dirname "$0")/.."
root="$PWD"
base="${1:-}"
if [[ ! -f "$base" ]]; then
 echo 'Usage: bash scripts/package-radio-from-v0910-installer.sh /absolute/path/to/MustardTube-v0.9.10-Explore-Home-REISSUED.muxapp' >&2;exit 2
fi
base="$(readlink -f "$base")"
binary="$root/build-arm64-ubuntu/mustardtube"
[[ -s "$binary" ]] || { echo 'Compile with scripts/build-arm64-ubuntu.sh first.' >&2;exit 2; }
file "$binary" | grep -Eq 'aarch64|ARM aarch64' || { echo 'Expected ARM64 executable' >&2;exit 2; }
unzip -tq "$base" >/dev/null
tmp="$(mktemp -d)";trap 'rm -rf "$tmp"' EXIT
unzip -q "$base" -d "$tmp"
app="$tmp/MustardTube"
[[ -s "$app/bin/node" && -f "$app/mux_launch.sh" && -s "$app/bin/mustardtube" ]] || { echo 'Base installer not recognized';exit 2; }
cp "$binary" "$app/bin/mustardtube"
cp scripts/youtube-backend.sh scripts/radio-backend.js "$app/scripts/"
mkdir -p "$app/assets/flags"
cp assets/flags/country-atlas.png assets/flags/codes.txt assets/flags/ATTRIBUTION.txt "$app/assets/flags/"
cp packaging/mux_launch.sh "$app/mux_launch.sh"
chmod 755 "$app/bin/mustardtube" "$app/mux_launch.sh" "$app/scripts/youtube-backend.sh"
chmod 644 "$app/scripts/radio-backend.js"
mkdir -p "$root/dist"
out="$root/dist/MustardTube-v0.9.11-Combined-Beta2-RadioUX-Andromeda-ARM64.muxapp"
(cd "$tmp" && zip -qr "$out" MustardTube)
unzip -tq "$out" >/dev/null
# Check critical playback dependencies match the base installer exactly.
for f in bin/node bin/yt-dlp bin/qjs assets/themes/midnight.theme licenses/USER_NOTICE.txt; do
 cmp -s <(unzip -p "$base" "MustardTube/$f") <(unzip -p "$out" "MustardTube/$f") || { echo "Unexpected difference: $f";exit 1; }
done
sha256sum "$out" > "$out.sha256"
echo "Created $out"
