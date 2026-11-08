#!/usr/bin/env bash
# Host-only lifecycle test; no controller or audio hardware required.
set -euo pipefail
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
tmp=$(mktemp -d)
app="$tmp/MustardTube"
mkdir -p "$app/bin" "$app/data" "$app/scripts" "$tmp/mockbin"
"$CC" -std=c11 -Os -Wall -Wextra src/radio_hotkeys.c src/game_mixer.c -o "$app/bin/radio-hotkeys"
"$app/bin/radio-hotkeys" --self-test
cp scripts/background-radio.js "$app/scripts/"
cat > "$app/bin/ffplay-radio" <<'PLAYER'
#!/bin/sh
exec sleep 180
PLAYER
cat > "$tmp/mockbin/wpctl" <<'MIX'
#!/bin/sh
printf '%s\n' "$*" >> "$MT_RADIO_VOLUME_LOG"
MIX
chmod +x "$app/bin/ffplay-radio" "$tmp/mockbin/wpctl"
export MT_RADIO_TEST_PLAYER=1 MUSTARDTUBE_APP_DIR="$app" MUSTARDTUBE_DATA_DIR="$app/data"
export MT_RADIO_VOLUME_LOG="$tmp/levels.txt" PATH="$tmp/mockbin:$PATH"
cleanup(){
  if [[ -f "$app/scripts/background-radio.js" ]];then node "$app/scripts/background-radio.js" stop >/dev/null 2>&1 || true;fi
  rm -rf "$tmp"
}
trap cleanup EXIT
[[ "$(node "$app/scripts/background-radio.js" get-hotkeys)" == 0 ]]
if node "$app/scripts/background-radio.js" set-hotkeys '1;echo fail' >/dev/null 2>&1;then echo 'FAIL: bad toggle accepted' >&2;exit 1;fi
printf 'Example Radio\nhttps://radio.example.org/live.mp3\n' > "$app/data/radio-selected.txt"
node "$app/scripts/background-radio.js" start >/dev/null
pid=$(node -p "require('$app/data/radio-background-state.json').pid")
# Disabled on first use: there must be no resident radio-hotkeys monitor.
sleep .2
[[ "$("$app/bin/radio-hotkeys" --status "$app")" == 'monitor=inactive' ]] || {
  echo 'FAIL: monitor started despite disabled preference' >&2;exit 1;
}
# Enable after the audio process is already playing: helper must attach.
node "$app/scripts/background-radio.js" set-hotkeys 1 >/dev/null
[[ "$(node "$app/scripts/background-radio.js" get-hotkeys)" == 1 ]]
for i in $(seq 1 40);do [[ -f "$app/data/radio-hotkeys.lock" ]]&&break;sleep .1;done
[[ -f "$app/data/radio-hotkeys.lock" ]] || { echo 'FAIL: monitor did not start' >&2;exit 1; }
[[ "$("$app/bin/radio-hotkeys" --status "$app")" == 'monitor=active' ]] || {
  echo 'FAIL: helper did not acquire app-owned monitor lock' >&2;exit 1;
}
# New mode 2 uses the SAME resident helper, not another process.
node "$app/scripts/background-radio.js" set-hotkeys 2 >/dev/null
[[ "$(node "$app/scripts/background-radio.js" get-hotkeys)" == 2 ]]
[[ "$("$app/bin/radio-hotkeys" --status "$app")" == 'monitor=active' ]]
# A newly enabled preference must be accepted even if set while radio is live.
node "$app/scripts/background-radio.js" set-hotkeys 0 >/dev/null
for i in $(seq 1 30);do
  [[ "$("$app/bin/radio-hotkeys" --status "$app")" == 'monitor=inactive' ]] && break
  sleep .1
done
[[ "$("$app/bin/radio-hotkeys" --status "$app")" == 'monitor=inactive' ]] || {
  echo 'FAIL: monitor remained resident after disabling' >&2;exit 1;
}
node "$app/scripts/background-radio.js" set-hotkeys 2 >/dev/null
for i in $(seq 1 30);do
  [[ "$("$app/bin/radio-hotkeys" --status "$app")" == 'monitor=active' ]] && break
  sleep .1
done
[[ "$(node "$app/scripts/background-radio.js" get-hotkeys)" == 2 ]]
[[ "$("$app/bin/radio-hotkeys" --status "$app")" == 'monitor=active' ]] || {
  echo 'FAIL: monitor failed to restart on opt-in' >&2;exit 1;
}
# Switching stations rapidly must not strand radio without a monitor while
# the old helper is still holding its advisory lock.
first_pid=$pid
printf 'Next Radio\nhttps://radio.example.org/next.mp3\n' > "$app/data/radio-selected.txt"
node "$app/scripts/background-radio.js" start >/dev/null
pid=$(node -p "require('$app/data/radio-background-state.json').pid")
[[ "$pid" != "$first_pid" ]]
sleep 1.5
[[ "$("$app/bin/radio-hotkeys" --status "$app")" == 'monitor=active' ]] || {
  echo 'FAIL: lost in-game helper during quick station change' >&2;exit 1;
}
[[ "$(node "$app/scripts/background-radio.js" get-volume)" == 75 ]]
node "$app/scripts/background-radio.js" adjust-volume 5 >/dev/null
[[ "$(node "$app/scripts/background-radio.js" get-volume)" == 80 ]]
node "$app/scripts/background-radio.js" adjust-volume -5 >/dev/null
[[ "$(node "$app/scripts/background-radio.js" get-volume)" == 75 ]]
grep -q 'set-volume --pid .* 80%' "$tmp/levels.txt"
# Simulate deleting the application during an active stream. The already
# running helper must terminate only its own verified audio process group.
rm -rf "$app"
for i in $(seq 1 50);do
  if [[ ! -r "/proc/$pid/stat" ]] || grep -qE '^.*\) [ZX] ' "/proc/$pid/stat"; then break;fi
  sleep .1
done
if [[ -r "/proc/$pid/stat" ]] && ! grep -qE '^.*\) [ZX] ' "/proc/$pid/stat";then
  echo 'FAIL: background radio remained live after app deletion' >&2
  kill -TERM -- "-$pid" 2>/dev/null || true
  exit 1
fi
[[ ! -e "$app" ]]
echo 'PASS: 3-state opt-in, one monitor, saved radio volume, uninstall cleanup'
