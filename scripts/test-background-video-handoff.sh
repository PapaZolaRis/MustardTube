#!/usr/bin/env bash
# Regression: starting YouTube playback must stop a detached background radio.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
app="$tmp/MustardTube"
mkdir -p "$app/bin" "$app/data" "$app/scripts"
cp scripts/background-radio.js "$app/scripts/"
ln -s "$(command -v node)" "$app/bin/node"
cat > "$app/bin/ffplay-radio" <<'PLAYER'
#!/bin/sh
exec sleep 180
PLAYER
cat > "$app/bin/mustardtube" <<'UI'
#!/bin/sh
n=$(cat "$MUSTARDTUBE_APP_DIR/data/step" 2>/dev/null || echo 0)
case "$n" in
  0) printf 1 > "$MUSTARDTUBE_APP_DIR/data/step"
     printf 'Radio One\nhttps://radio.example.org/play\n' > "$MUSTARDTUBE_APP_DIR/data/radio-selected.txt"
     exit 84;;
  1) printf 2 > "$MUSTARDTUBE_APP_DIR/data/step"
     printf 'Example video\nExample video title\n' > "$MUSTARDTUBE_APP_DIR/data/selected.txt"
     exit 77;;
  2) printf 3 > "$MUSTARDTUBE_APP_DIR/data/step"; exit 0;;
esac
UI
cat > "$app/scripts/youtube-backend.sh" <<'BACKEND'
#!/bin/sh
case "$1" in
resolve)
  printf 'https://videos.example.org/test.mp4\n' > "$MUSTARDTUBE_APP_DIR/data/stream-url.txt"
  printf 'vod\n' > "$MUSTARDTUBE_APP_DIR/data/stream-kind.txt"
  exit 0;;
*) exit 0;;
esac
BACKEND
cat > "$tmp/fake-wasabi" <<'WASABI'
#!/bin/sh
# Confirm background radio has stopped before foreground playback begins.
if MT_RADIO_TEST_PLAYER=1 "$MUSTARDTUBE_APP_DIR/bin/node" "$MUSTARDTUBE_APP_DIR/scripts/background-radio.js" status >/dev/null 2>&1;then
  exit 70
fi
printf 'video started after background stopped\n' > "$MUSTARDTUBE_APP_DIR/data/video-handoff-checked.txt"
exit 0
WASABI
chmod +x "$app/bin/ffplay-radio" "$app/bin/mustardtube" "$app/scripts/youtube-backend.sh" "$tmp/fake-wasabi"
sed -e 's@^\. /opt/muos/script/var/func.sh@SETUP_APP() { :; }@' \
 -e "s@^APP_DIR=.*@APP_DIR=\"$app\"@" \
 -e "s@^WASABI=.*@WASABI=\"$tmp/fake-wasabi\"@" packaging/mux_launch.sh > "$tmp/launcher.sh"
cleanup(){ MT_RADIO_TEST_PLAYER=1 MUSTARDTUBE_APP_DIR="$app" MUSTARDTUBE_DATA_DIR="$app/data" \
  node "$app/scripts/background-radio.js" stop >/dev/null 2>&1 || true; rm -rf "$tmp"; }
trap cleanup EXIT
MT_RADIO_TEST_PLAYER=1 MUSTARDTUBE_APP_DIR="$app" sh "$tmp/launcher.sh"
[[ -f "$app/data/video-handoff-checked.txt" ]]
[[ ! -e "$app/data/radio-background-state.json" ]]
[[ "$(cat "$app/data/step")" = 3 ]]
echo 'PASS: video stops background radio before invoking Wasabi'
