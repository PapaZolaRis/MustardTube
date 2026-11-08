#!/usr/bin/env bash
# Developer-only: patch a KNOWN-GOOD prebuilt Beta 2.1 installer, retaining
# its proven Node/yt-dlp runtime. Source archive alone cannot produce .muxapp.
set -euo pipefail
cd "$(dirname "$0")/.."
root="$PWD"
base="${1:-}"
[[ -f "$base" ]] || { echo 'Usage: package-beta22-from-beta21-installer.sh /path/to/Beta2.1.muxapp' >&2;exit 2; }
base=$(realpath "$base")
appbin="$root/build-arm64-ubuntu/mustardtube"
# Guard against an ARM64 binary built before the latest SDL navigation change.
if [[ -s "$appbin" && "$root/src/main.c" -nt "$appbin" ]]; then
  echo "ERROR: MustardTube ARM64 binary is older than src/main.c." >&2
  echo "Run bash scripts/build-arm64-ubuntu.sh before packaging." >&2
  exit 2
fi
player="$root/tools/ffplay-radio"
hotkeys="$root/tools/radio-hotkeys"
# Ensure the rebuilt helper is newer than the faster hold-repeat sources.
for source in "$root/src/radio_hotkeys.c" "$root/src/game_mixer.c" "$root/src/game_mixer.h";do
  if [[ -s "$hotkeys" && "$source" -nt "$hotkeys" ]];then
    echo 'ERROR: ARM64 radio-hotkeys binary is older than the hold-repeat source.' >&2
    echo 'Run bash scripts/build-radio-hotkeys-arm64.sh before packaging.' >&2
    exit 2
  fi
done
for f in "$appbin" "$player" "$hotkeys" "$root/tools/FFPLAY_LGPL_LICENSE.txt" "$root/tools/FFPLAY_SOURCE.txt";do
  [[ -s "$f" ]] || { echo "Missing required compiled component: $f" >&2;exit 2; }
done
for f in "$appbin" "$player" "$hotkeys";do
  file "$f" | grep -Eq 'aarch64|ARM aarch64' || { echo "Expected ARM64: $f" >&2;exit 2; }
done
# Protect against mixing the old Beta 2.2.2 helper with this release.
if ! strings "$hotkeys" | grep -Fq 'single-press and hold-to-repeat';then
  echo 'ERROR: packaged radio-hotkeys lacks hold-to-repeat. Rebuild the helper.' >&2
  exit 2
fi
bash scripts/audit-player-arm64.sh "$player"
unzip -tq "$base" >/dev/null
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
unzip -q "$base" -d "$tmp"
app="$tmp/MustardTube"
[[ -s "$app/bin/node" && -s "$app/bin/yt-dlp" && -s "$app/bin/mustardtube" && -r "$app/mux_launch.sh" ]] || {
  echo 'Not a compatible prebuilt MustardTube installer' >&2;exit 2;
}
cp "$appbin" "$app/bin/mustardtube"
cp "$player" "$app/bin/ffplay-radio"
cp "$hotkeys" "$app/bin/radio-hotkeys"
cp packaging/mux_launch.sh "$app/mux_launch.sh"
cp scripts/background-radio.js "$app/scripts/"
cp scripts/radio-backend.js scripts/youtube-backend.sh "$app/scripts/"
mkdir -p "$app/assets/flags" "$app/licenses"
cp assets/flags/country-atlas.png assets/flags/codes.txt assets/flags/ATTRIBUTION.txt "$app/assets/flags/"
cp tools/FFPLAY_LGPL_LICENSE.txt tools/FFPLAY_SOURCE.txt "$app/licenses/"
[[ ! -s tools/FFPLAY_GPL_LICENSE.txt ]] || cp tools/FFPLAY_GPL_LICENSE.txt "$app/licenses/"
chmod 755 "$app/bin/mustardtube" "$app/bin/ffplay-radio" "$app/bin/radio-hotkeys" "$app/mux_launch.sh" "$app/scripts/youtube-backend.sh"
chmod 644 "$app/scripts/background-radio.js"
mkdir -p "$root/dist"
out="$root/dist/MustardTube-v0.9.11-Beta2.2.6a-DualMixer-StaleSessionFix-UI-EXPERIMENTAL-Andromeda-ARM64.muxapp"
(cd "$tmp" && zip -qr "$out" MustardTube)
unzip -tq "$out" >/dev/null
# Preserve stock known-good YouTube extraction/runtime. Do not silently rewrite it.
for f in bin/node bin/yt-dlp bin/qjs;do
  cmp -s <(unzip -p "$base" "MustardTube/$f") <(unzip -p "$out" "MustardTube/$f") || {
    echo "Changed baseline component $f; package rejected" >&2;exit 1;
  }
done
sha256sum "$out" > "$out.sha256"
echo "Created experimental installer: $out"
echo 'Not validated on a physical muOS device. Test first; retain Beta 2.1 backup.'
