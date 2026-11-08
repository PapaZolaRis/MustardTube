#!/usr/bin/env bash
# Offline mock PipeWire test. Does not contact network or device audio.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
app="$tmp/MustardTube"
mkdir -p "$app/bin" "$app/data" "$app/scripts" "$tmp/mockbin"
cp scripts/background-radio.js "$app/scripts/"
cat > "$app/bin/ffplay-radio" <<'PLAYER'
#!/bin/sh
exec sleep 90
PLAYER
cat > "$tmp/mockbin/wpctl" <<'MIX'
#!/bin/sh
printf '%s\n' "$*" >> "$MT_RADIO_VOLUME_LOG"
MIX
chmod +x "$app/bin/ffplay-radio" "$tmp/mockbin/wpctl"
cleanup(){ MT_RADIO_TEST_PLAYER=1 MUSTARDTUBE_APP_DIR="$app" MUSTARDTUBE_DATA_DIR="$app/data" \
 PATH="$tmp/mockbin:$PATH" MT_RADIO_VOLUME_LOG="$tmp/levels.txt" \
 node "$app/scripts/background-radio.js" stop >/dev/null 2>&1 || true; rm -rf "$tmp"; }
trap cleanup EXIT
export MT_RADIO_TEST_PLAYER=1 MUSTARDTUBE_APP_DIR="$app" MUSTARDTUBE_DATA_DIR="$app/data"
export MT_RADIO_VOLUME_LOG="$tmp/levels.txt" PATH="$tmp/mockbin:$PATH"
[[ "$(node "$app/scripts/background-radio.js" get-volume)" = 75 ]]
printf 'Test Station\nhttps://radio.example.org/live.mp3\n' > "$app/data/radio-selected.txt"
node "$app/scripts/background-radio.js" start >/dev/null
# Async process registration: bounded startup helper applies saved value.
for i in $(seq 1 50);do grep -q 'set-volume --pid .* 75%' "$tmp/levels.txt" 2>/dev/null && break; sleep .1;done
grep -q 'set-volume --pid .* 75%' "$tmp/levels.txt"
node "$app/scripts/background-radio.js" set-volume 80 >/dev/null
[[ "$(node "$app/scripts/background-radio.js" get-volume)" = 80 ]]
grep -q 'set-volume --pid .* 80%' "$tmp/levels.txt"
[[ "$(cat "$app/data/radio-mix-volume.txt")" = 80 ]]
if node "$app/scripts/background-radio.js" set-volume '30;echo bad' >/dev/null 2>&1;then
  echo 'FAIL: untrusted volume accepted' >&2; exit 1
fi
[[ "$(node "$app/scripts/background-radio.js" get-volume)" = 80 ]]
node "$app/scripts/background-radio.js" stop >/dev/null
[[ "$(node "$app/scripts/background-radio.js" get-volume)" = 80 ]]
[[ ! -e "$app/data/radio-background-state.json" ]]
if grep -q 'radio.example.org' "$app/data/radio-mix-volume.txt" "$tmp/levels.txt";then
  echo 'FAIL: stream URL leaked to volume persistence' >&2; exit 1
fi
printf 'broken\n' > "$app/data/radio-mix-volume.txt"
[[ "$(node "$app/scripts/background-radio.js" get-volume)" = 75 ]]
echo 'PASS: 75% default, async startup mix, live 80% adjustment, persistence, invalid input, no URLs'
