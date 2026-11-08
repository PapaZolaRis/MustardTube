#!/usr/bin/env bash
# End-to-end launcher simulation: native FFplay path, names, stop/cleanup, no Wasabi.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
app="$tmp/MustardTube"
mkdir -p "$app/bin" "$app/scripts" "$app/data"
cp scripts/background-radio.js "$app/scripts/"
ln -s "$(command -v node)" "$app/bin/node"
cat > "$app/bin/ffplay-radio" <<'PLAYER'
#!/bin/sh
exec sleep 180
PLAYER
cat > "$app/bin/mustardtube" <<'APP'
#!/bin/sh
n=$(cat "$MUSTARDTUBE_APP_DIR/data/number" 2>/dev/null || echo 0)
case "$n" in
  0)
    echo 1 > "$MUSTARDTUBE_APP_DIR/data/number"
    printf 'Test Station\nhttps://radio.example.org/audio.mp3\n' > "$MUSTARDTUBE_APP_DIR/data/radio-selected.txt"
    printf 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa\nUS\nMP3\nTest Station\n' > "$MUSTARDTUBE_APP_DIR/data/radio-now-meta.txt"
    exit 86;;
  1)
    test "$*" = '--assets '"$MUSTARDTUBE_APP_DIR"'/assets --show-radio-now' || exit 51
    grep -qx 'Test Station' "$MUSTARDTUBE_APP_DIR/data/radio-background-name.txt" || exit 52
    echo 2 > "$MUSTARDTUBE_APP_DIR/data/number"
    exit 87;;
  2)
    test "$*" = '--assets '"$MUSTARDTUBE_APP_DIR"'/assets --show-radio-now' || exit 53
    test ! -s "$MUSTARDTUBE_APP_DIR/data/radio-background-name.txt" || exit 54
    echo 3 > "$MUSTARDTUBE_APP_DIR/data/number"
    exit 0;;
esac
exit 55
APP
chmod 755 "$app/bin/ffplay-radio" "$app/bin/mustardtube"
cat > "$tmp/wasabi" <<'WASABI'
#!/bin/sh
printf 'BAD: Wasabi launched for native radio\n' >&2
exit 80
WASABI
chmod 755 "$tmp/wasabi"
sed -e 's@^\. /opt/muos/script/var/func.sh@SETUP_APP() { :; }@' \
  -e "s@^APP_DIR=.*@APP_DIR=\"$app\"@" \
  -e "s@^WASABI=.*@WASABI=\"$tmp/wasabi\"@" \
  packaging/mux_launch.sh > "$tmp/launcher.sh"
cleanup(){ MT_RADIO_TEST_PLAYER=1 MUSTARDTUBE_APP_DIR="$app" \
  MUSTARDTUBE_DATA_DIR="$app/data" node "$app/scripts/background-radio.js" stop >/dev/null 2>&1 || true
  rm -rf "$tmp"; }
trap cleanup EXIT
MT_RADIO_TEST_PLAYER=1 MUSTARDTUBE_APP_DIR="$app" sh "$tmp/launcher.sh"
test "$(cat "$app/data/number")" = '3'
test ! -e "$app/data/radio-selected.txt"
test ! -e "$app/data/radio-background-state.json"
! grep -q 'https://radio.example.org' "$app/data/launch.log"
echo 'PASS: native Now Playing: one FFplay stream, named station, stop and return, no Wasabi or URL logs'
