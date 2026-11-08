#!/usr/bin/env bash
# Host-only lifecycle regression: process EXIT -> stop radio -> same emulator
# reopened under a NEW PID.  Never touches any real audio sink.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d)
old= new= wrong=
cleanup(){
  for p in "${old:-}" "${new:-}" "${wrong:-}";do
    if [[ "$p" =~ ^[0-9]+$ ]];then kill "$p" 2>/dev/null || true;wait "$p" 2>/dev/null || true;fi
  done
  rm -rf "$work"
}
trap cleanup EXIT
mkdir -p "$work/app/data" "$work/app/bin" "$work/app/scripts" "$work/mockbin"
${CC:-cc} -std=c11 -O2 -Wall -Wextra -Wpedantic \
  src/game_mixer.c src/radio_hotkeys.c -o "$work/app/bin/radio-hotkeys"
${CC:-cc} -std=c11 -O2 -Wall -Wextra -Wpedantic \
  src/game_mixer.c tests/game-mixer-harness.c -o "$work/mixer"
cp scripts/background-radio.js "$work/app/scripts/"
printf '0.95\n' > "$work/volume"
echo old > "$work/stage"
cat > "$work/mockbin/wpctl" <<'MOCK'
#!/bin/sh
set -eu
stage=$(cat "$MT_RECOVERY_TEST/stage")
old=$(cat "$MT_RECOVERY_TEST/old")
case "$1" in
 status)
   cat <<'EOF'
PipeWire 'pipewire-0'
Audio
 ├─ Sinks:
 │      32. Built-in Audio Stereo
 └─ Streams:
EOF
   case "$stage" in
     old) echo '        42. PipeWire ALSA [PPSSPP]' ;;
     same) echo '        44. PipeWire ALSA [PPSSPP]' ;;
     wrong) echo '        55. PipeWire ALSA [Other program]' ;;
     *) :;;
   esac
   printf 'Video\n └─ Streams:\n'
   ;;
 inspect)
   case "$2" in
     42) pid=$old;;
     44) pid=$(cat "$MT_RECOVERY_TEST/new");;
     55) pid=$(cat "$MT_RECOVERY_TEST/wrong");;
     *) exit 1;;
   esac
   printf 'media.class = "Stream/Output/Audio"\napplication.process.id = "%s"\n' "$pid";;
 get-volume)
   case "$2" in 42|44) printf 'Volume: %s\n' "$(cat "$MT_RECOVERY_TEST/volume")";; *) exit 1;; esac;;
 set-volume)
   case "$2" in
     42|44) printf '%s %s\n' "$2" "$3" >> "$MT_RECOVERY_TEST/writes";
            printf '%s\n' "$3" > "$MT_RECOVERY_TEST/volume";;
     *) echo "UNSAFE $*" >> "$MT_RECOVERY_TEST/unsafe";exit 1;;
   esac;;
 *) exit 1;;
esac
MOCK
chmod +x "$work/mockbin/wpctl"
export MT_RECOVERY_TEST="$work" PATH="$work/mockbin:$PATH"
app="$work/app"
sleep 120 & old=$!; echo "$old" > "$work/old"
# Initial adjustment from 95% to 90%, recording the old executable inode.
"$work/mixer" down "$app" 99999
[[ $(cat "$work/volume") == '0.9000' ]]
[[ $(wc -w < "$app/data/game-volume.session") == 6 ]]
kill "$old";wait "$old" 2>/dev/null || true; old=
# Old process has vanished. Restore MUST remain pending, not be thrown away.
if "$app/bin/radio-hotkeys" --restore-game "$app";then
  echo 'FAIL: declared restoration of an exited process' >&2;exit 1
else [[ "$?" == 3 ]]
fi
[[ -f "$app/data/game-volume.session" ]]
"$app/bin/radio-hotkeys" --game-session "$app" | grep -q 'game_restore_state=awaiting_reopen'
if "$app/bin/radio-hotkeys" --recover-once "$app";then
  echo 'FAIL: restored a nonexistent stream' >&2;exit 1
else [[ "$?" == 3 ]]
fi
# A different executable with a new audio stream MUST NOT receive the level.
tail -f /dev/null >/dev/null 2>&1 & wrong=$!;echo "$wrong" > "$work/wrong"
echo wrong > "$work/stage"
if "$app/bin/radio-hotkeys" --recover-once "$app";then
  echo 'FAIL: restored unrelated emulator' >&2;exit 1
else [[ "$?" == 3 ]]
fi
[[ $(cat "$work/volume") == '0.9000' ]]
[[ $(wc -l < "$work/writes") == 1 ]]
kill "$wrong";wait "$wrong" 2>/dev/null || true;wrong=
# Same executable, DIFFERENT PID, unique valid audio stream: restore now.
sleep 120 & new=$!;echo "$new" > "$work/new"
echo same > "$work/stage"
"$app/bin/radio-hotkeys" --recover-once "$app"
[[ $(cat "$work/volume") == '0.9500' ]]
[[ ! -f "$app/data/game-volume.session" ]]
[[ $(tail -n1 "$work/writes") == '44 0.9500' ]]
[[ ! -f "$work/unsafe" ]]
kill "$new";wait "$new" 2>/dev/null || true;new=
# Reproduce through actual Node STOP: it must spawn the SAME binary in
# bounded recovery mode.  Mock radio is absent because we only test stop.
sleep 120 & old=$!;echo "$old" > "$work/old"
echo old > "$work/stage"
"$work/mixer" down "$app" 99999
[[ $(cat "$work/volume") == '0.9000' ]]
kill "$old";wait "$old" 2>/dev/null || true;old=
node "$app/scripts/background-radio.js" stop >/dev/null
[[ -f "$app/data/game-volume.session" ]]
sleep 120 & new=$!;echo "$new" > "$work/new"
echo same > "$work/stage"
for i in $(seq 1 45);do
  [[ ! -e "$app/data/game-volume.session" ]] && break
  sleep 0.2
done
[[ ! -e "$app/data/game-volume.session" ]]
[[ $(cat "$work/volume") == '0.9500' ]]
[[ ! -f "$work/unsafe" ]]
echo 'PASS: exit-before-stop retains original volume; rejects wrong emulator; restores same executable on reopen; bounded Node stop recovery'
