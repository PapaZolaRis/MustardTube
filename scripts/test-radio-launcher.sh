#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT
app="$tmp/MustardTube";mkdir -p "$app/bin" "$app/data";printf 'dummy\n' > "$app/mux_lang.ini"
cat > "$app/bin/mustardtube" <<'APP'
#!/bin/sh
printf '%s\n' "$*" >> "$MUSTARDTUBE_APP_DIR/data/app-invocations.log"
if [ -e "$MUSTARDTUBE_APP_DIR/data/done" ]; then exit 0;fi
: > "$MUSTARDTUBE_APP_DIR/data/done"
printf 'Example Station\nhttps://radio.example.org/live.mp3\n' > "$MUSTARDTUBE_APP_DIR/data/radio-selected.txt"
exit 82
APP
chmod +x "$app/bin/mustardtube"
cat > "$tmp/wasabi" <<'APP'
#!/bin/sh
printf '%s\n' "$*" > "$MUSTARDTUBE_APP_DIR/data/wasabi-args.txt"
exit 0
APP
chmod +x "$tmp/wasabi"
# Simulate the muOS functions and binary paths; never run against device files.
sed -e 's@^\. /opt/muos/script/var/func.sh@SETUP_APP() { :; }@' \
 -e "s@^APP_DIR=.*@APP_DIR=\"$app\"@" \
 -e "s@^WASABI=.*@WASABI=\"$tmp/wasabi\"@" \
 packaging/mux_launch.sh > "$tmp/launcher.sh"
MUSTARDTUBE_APP_DIR="$app" sh "$tmp/launcher.sh"
grep -q -- '--live --title Example Station https://radio.example.org/live.mp3' "$app/data/wasabi-args.txt"
grep -q -- '--show-radio' "$app/data/app-invocations.log"
[ ! -f "$app/data/radio-selected.txt" ]
if grep -Fq 'https://radio.example.org/live.mp3' "$app/data/launch.log";then
 echo 'FAIL: stream URL leaked into launcher diagnostics' >&2;exit 1
fi
echo 'PASS: radio launcher invokes stock Wasabi, returns to Radio, removes selected URL, avoids URL logging'
