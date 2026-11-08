#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
app="$tmp/MustardTube"
mkdir -p "$app/bin" "$app/data" "$app/scripts"
cp scripts/background-radio.js "$app/scripts/"
# Fake audio-only player never makes a network call; test opt-in bypasses ELF check.
cat > "$app/bin/ffplay-radio" <<'FAKE'
#!/bin/sh
exec sleep 180
FAKE
chmod +x "$app/bin/ffplay-radio"
cleanup(){
  MT_RADIO_TEST_PLAYER=1 MUSTARDTUBE_APP_DIR="$app" MUSTARDTUBE_DATA_DIR="$app/data" \
    node "$app/scripts/background-radio.js" stop >/dev/null 2>&1 || true
  rm -rf "$tmp"
}
trap cleanup EXIT
printf 'Example Station\nhttps://radio.example.org/live.mp3\n' > "$app/data/radio-selected.txt"
MT_RADIO_TEST_PLAYER=1 MUSTARDTUBE_APP_DIR="$app" MUSTARDTUBE_DATA_DIR="$app/data" \
  node "$app/scripts/background-radio.js" start | grep -q 'Started background radio'
[ ! -e "$app/data/radio-selected.txt" ]
grep -q '"station":"Example Station"' "$app/data/radio-background-state.json"
if grep -q 'radio.example.org' "$app/data/radio-background-state.json"; then
  echo 'FAIL: persisted stream URL' >&2; exit 1
fi
name=$(MT_RADIO_TEST_PLAYER=1 MUSTARDTUBE_APP_DIR="$app" MUSTARDTUBE_DATA_DIR="$app/data" node "$app/scripts/background-radio.js" status)
[ "$name" = 'Example Station' ]
pid=$(node -p "require('$app/data/radio-background-state.json').pid")
kill -0 "$pid"
MT_RADIO_TEST_PLAYER=1 MUSTARDTUBE_APP_DIR="$app" MUSTARDTUBE_DATA_DIR="$app/data" \
  node "$app/scripts/background-radio.js" stop >/dev/null
[ ! -e "$app/data/radio-background-state.json" ]
# Invalid URL is rejected and selection is removed even on failure.
printf 'Bad Station\nfile:///tmp/private.mp3\n' > "$app/data/radio-selected.txt"
if MT_RADIO_TEST_PLAYER=1 MUSTARDTUBE_APP_DIR="$app" MUSTARDTUBE_DATA_DIR="$app/data" \
  node "$app/scripts/background-radio.js" start 2>/dev/null; then
  echo 'FAIL: invalid stream accepted' >&2; exit 1
fi
[ ! -e "$app/data/radio-selected.txt" ]
# An absent bundled player is a visible, safe failure, not a Wasabi fallback.
rm "$app/bin/ffplay-radio"
printf 'Example Station\nhttps://radio.example.org/live.mp3\n' > "$app/data/radio-selected.txt"
if MT_RADIO_TEST_PLAYER=1 MUSTARDTUBE_APP_DIR="$app" MUSTARDTUBE_DATA_DIR="$app/data" \
  node "$app/scripts/background-radio.js" start 2>/dev/null; then
  echo 'FAIL: missing player accepted' >&2; exit 1
fi
[ ! -e "$app/data/radio-selected.txt" ]
echo 'PASS: background process lifecycle, PID identity, no stream URL persistence, invalid URL, missing player'
