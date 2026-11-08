#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
app="$tmp/MustardTube"
mkdir -p "$app/bin" "$app/data" "$app/scripts"
cp scripts/background-radio.js "$app/scripts/"
ln -s "$(command -v node)" "$app/bin/node"
cat > "$app/bin/ffplay-radio" <<'FAKE'
#!/bin/sh
exec sleep 180
FAKE
cat > "$app/bin/mustardtube" <<'APP'
#!/bin/sh
file="$MUSTARDTUBE_APP_DIR/data/num"
num=$(cat "$file" 2>/dev/null || echo 0)
case "$num" in
  0)
    echo 1 > "$file"
    printf 'Example Station\nhttps://radio.example.org/live.mp3\n' > "$MUSTARDTUBE_APP_DIR/data/radio-selected.txt"
    exit 84;;
  1)
    echo 2 > "$file"
    grep -qx 'Example Station' "$MUSTARDTUBE_APP_DIR/data/radio-background-name.txt" || exit 50
    exit 85;;
  2)
    echo 3 > "$file"
    ! grep -q 'Example Station' "$MUSTARDTUBE_APP_DIR/data/radio-background-name.txt" || exit 51
    exit 0;;
esac
APP
chmod 755 "$app/bin/mustardtube" "$app/bin/ffplay-radio"
# Replace only absolute device paths; the real installed launcher remains untouched.
sed -e 's@^\. /opt/muos/script/var/func.sh@SETUP_APP() { :; }@' \
  -e "s@^APP_DIR=.*@APP_DIR=\"$app\"@" packaging/mux_launch.sh > "$tmp/launcher.sh"
cleanup(){ MT_RADIO_TEST_PLAYER=1 MUSTARDTUBE_APP_DIR="$app" \
  MUSTARDTUBE_DATA_DIR="$app/data" node "$app/scripts/background-radio.js" stop >/dev/null 2>&1 || true;
  rm -rf "$tmp"; }
trap cleanup EXIT
MT_RADIO_TEST_PLAYER=1 MUSTARDTUBE_APP_DIR="$app" sh "$tmp/launcher.sh"
[[ "$(cat "$app/data/num")" = 3 ]]
[[ ! -e "$app/data/radio-selected.txt" ]]
[[ ! -e "$app/data/radio-background-state.json" ]]
if grep -q 'https://radio.example.org' "$app/data/launch.log";then
  echo 'FAIL: leaked radio URL' >&2; exit 1
fi
echo 'PASS: launcher start -> named status -> stop -> return to app; no URL logging'
