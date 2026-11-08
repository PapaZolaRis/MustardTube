#!/usr/bin/env bash
# Host-only game mixer tests. No PipeWire, audio driver or real games required.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'kill "${game:-}" "${game2:-}" "${old_game:-}" "${other_game:-}" "${reopened_game:-}" 2>/dev/null || true; rm -rf "$work"' EXIT
mkdir -p "$work/app/data" "$work/mockbin"
${CC:-cc} -std=c11 -O2 -Wall -Wextra -Wpedantic src/game_mixer.c tests/game-mixer-harness.c -o "$work/mixer"
sleep 120 & game=$!
sleep 120 & game2=$!
export MT_GAME_PID="$game" MT_OTHER_PID="$game2" MT_MIX_TEST="$work" PATH="$work/mockbin:$PATH"
echo 1.00 > "$work/vol"
cat > "$work/mockbin/wpctl" <<'MOCK'
#!/bin/sh
set -eu
case "$1" in
  status)
    cat <<EOF_STATUS
PipeWire 'pipewire-0'
Audio
 ├─ Sinks:
 │      32. Built-in Audio Stereo
 └─ Streams:
EOF_STATUS
    if [ "${MT_RELOCATE_NODE:-0}" = 1 ];then
        printf '        55. PipeWire ALSA [Game: recreated stream]\n'
    else
        printf '        42. PipeWire ALSA [Game: unfamiliar emulator]\n             44. output_FL       > Stereo:playback_FL [active]\n'
    fi
    printf '        48. PipeWire ALSA [ffplay-radio]\n'
    if [ "${MT_EXTRA_STREAM:-0}" = 1 ];then
        printf '        56. Custom Audio [Other application]\n'
    fi
    printf '\nVideo\n └─ Streams:\n'
    ;;
  inspect)
    if [ "${MT_FAIL_INSPECT:-0}" = 1 ];then exit 1;fi
    case "$2" in
      42)
        [ "${MT_RELOCATE_NODE:-0}" != 1 ] || exit 1
        target=${MT_OVERRIDE_GAME_PID:-$MT_GAME_PID}
        [ ! -s "$MT_MIX_TEST/override-pid" ] || target=$(cat "$MT_MIX_TEST/override-pid")
        printf 'id 42\n    media.class = "Stream/Output/Audio"\n    application.process.id = "%s"\n' "$target";;
      48) printf 'id 48\n    media.class = "Stream/Output/Audio"\n    application.process.id = "%s"\n' "${MT_RADIO_PID:-99999}";;
      55)
        [ "${MT_RELOCATE_NODE:-0}" = 1 ] || exit 1
        printf 'id 55\n    media.class = "Stream/Output/Audio"\n    application.process.id = "%s"\n' "${MT_OVERRIDE_GAME_PID:-$MT_GAME_PID}";;
      56) printf 'id 56\n    media.class = "Stream/Output/Audio"\n    application.process.id = "%s"\n' "$MT_OTHER_PID";;
      *) exit 1;;
    esac
    ;;
  get-volume)
    case "$2" in
      42|55) printf 'Volume: %s\n' "$(cat "$MT_MIX_TEST/vol")";;
      56) printf 'Volume: 1.00\n';;
      *) exit 1;;
    esac
    ;;
  set-volume)
    if [ "$2" = --pid ];then
      printf '%s\n' "$*" >> "$MT_MIX_TEST/radio-writes"
      exit 0
    fi
    if [ "$2" != 42 ] && ! { [ "$2" = 55 ] && [ "${MT_RELOCATE_NODE:-0}" = 1 ]; }; then
        echo "UNSAFE NODE: $2" >> "$MT_MIX_TEST/unsafe"; exit 1;
    fi
    printf '%s %s\n' "$2" "$3" >> "$MT_MIX_TEST/writes"
    echo "$3" > "$MT_MIX_TEST/vol"
    ;;
  *) exit 1;;
esac
MOCK
chmod +x "$work/mockbin/wpctl"
app="$work/app"
# Standalone helper CLI validates user-supplied mix levels without shelling
# out to a global/master volume control or starting a second resident process.
${CC:-cc} -std=c11 -O2 src/radio_hotkeys.c src/game_mixer.c -o "$work/mix-cli"
"$work/mix-cli" --set-game-mix "$app" 40
[[ "$(cat "$app/data/game-mix-volume.txt")" == 40 ]]
if "$work/mix-cli" --set-game-mix "$app" 120;then
  echo 'FAIL: accepted out-of-range game volume' >&2;exit 1
fi
[[ "$(cat "$app/data/game-mix-volume.txt")" == 40 ]]
"$work/mix-cli" --set-game-mix "$app" 100
# Unique game stream discovered by process identity, not emulator name.
"$work/mixer" inspect "$app" 99999 | grep -q 'game_stream=unique node=42'
# A hold at the 100% cap must not produce a stream-volume write.
"$work/mixer" up-at-limit "$app" 99999
[[ "$(cat "$work/vol")" == '1.00' ]]
[[ ! -s "$work/writes" ]]
"$work/mixer" restore "$app" 99999
[[ ! -e "$app/data/game-volume.session" ]]
"$work/mixer" down "$app" 99999
[[ "$(cat "$work/vol")" == '0.9500' ]]
[[ -f "$app/data/game-volume.session" ]]
# Diagnostic must be read-only and report the saved original level.
${CC:-cc} -std=c11 -O2 src/radio_hotkeys.c src/game_mixer.c -o "$work/radio-hotkeys-status"
"$work/radio-hotkeys-status" --game-session "$app" | grep -q 'game_restore_state=active'
"$work/mixer" down "$app" 99999
[[ "$(cat "$work/vol")" == '0.9000' ]]
# Reset affected game to 100% when helper or radio stops.
"$work/mixer" restore "$app" 99999
[[ "$(cat "$work/vol")" == '1.0000' ]]
[[ ! -e "$app/data/game-volume.session" ]]
# Saved radio-session GAME preference must be applied to the next game.
"$work/mixer" mix-set "$app" 65
[[ "$("$work/mixer" mix-get "$app" 99999)" == 'mix=65' ]]
echo 0.65 > "$work/vol"
"$work/mixer" up "$app" 99999
[[ "$(cat "$work/vol")" == '0.7000' ]]
"$work/mixer" restore "$app" 99999
[[ "$(cat "$work/vol")" == '1.0000' ]]
# Transient PipeWire lookup: never discard the game's recovery record.
"$work/mixer" mix-set "$app" 80
echo 0.80 > "$work/vol"
"$work/mixer" down "$app" 99999
[[ "$(cat "$work/vol")" == '0.7500' ]]
export MT_FAIL_INSPECT=1
if "$work/mixer" restore "$app" 99999; then echo 'FAIL: invalid lookup treated as restored' >&2;exit 1;fi
[[ -f "$app/data/game-volume.session" ]]
"$work/radio-hotkeys-status" --game-session "$app" | grep -q 'game_restore_state=pending_stream_unverified'
if "$work/mixer" down "$app" 99999; then echo 'FAIL: overwrote pending restoration' >&2;exit 1;fi
[[ "$(cat "$work/vol")" == '0.7500' ]]
unset MT_FAIL_INSPECT
"$work/mixer" restore "$app" 99999
[[ "$(cat "$work/vol")" == '1.0000' ]]
[[ ! -e "$app/data/game-volume.session" ]]
# Ambiguous audio streams: fail closed, no volume changes.
export MT_EXTRA_STREAM=1
if "$work/mixer" down "$app" 99999;then echo 'FAIL: ambiguous target accepted' >&2;exit 1;fi
[[ ! -e "$app/data/game-volume.session" ]]
unset MT_EXTRA_STREAM
# If a saved stream's process identity changes, never restore or adjust it.
"$work/mixer" mix-set "$app" 80
echo 0.80 > "$work/vol"
"$work/mixer" down "$app" 99999
[[ "$(cat "$work/vol")" == '0.7500' ]]
export MT_OVERRIDE_GAME_PID="$game2"
if "$work/mixer" restore "$app" 99999;then echo 'FAIL: wrong PID treated as successful restoration' >&2;exit 1;fi
[[ "$(cat "$work/vol")" == '0.7500' ]]
[[ -f "$app/data/game-volume.session" ]]
unset MT_OVERRIDE_GAME_PID
"$work/mixer" restore "$app" 99999
[[ "$(cat "$work/vol")" == '1.0000' ]]
[[ ! -e "$app/data/game-volume.session" ]]
# A game slider level chosen BEFORE launching should apply to a uniquely
# identified new game without requiring a game hotkey press.
"$work/mixer" mix-set "$app" 55
echo 1.00 > "$work/vol"
"$work/mixer" mix-sync "$app" 99999
[[ "$(cat "$work/vol")" == '0.5500' ]]
[[ -f "$app/data/game-volume.session" ]]
# Live UI slider changes propagate to the same validated stream.
"$work/mixer" mix-set "$app" 75
[[ "$(cat "$work/vol")" == '0.7500' ]]
"$work/mixer" down "$app" 99999
[[ "$(cat "$work/vol")" == '0.7000' ]]
[[ "$("$work/mixer" mix-get "$app" 99999)" == 'mix=70' ]]
"$work/mixer" restore "$app" 99999
[[ "$(cat "$work/vol")" == '1.0000' ]]
# When the identified stream belongs to radio, there is no game target.
export MT_RADIO_PID="$game" MT_OVERRIDE_GAME_PID="$game"
if "$work/mixer" up "$app" "$game";then echo 'FAIL: radio selected as game' >&2;exit 1;fi
[[ ! -e "$app/data/game-volume.session" ]]
[[ ! -f "$work/unsafe" ]]
[[ ! -s "$work/writes" ]] && { echo 'FAIL: tests never wrote a game volume' >&2;exit 1; }
# End-to-end restoration via the EXISTING background-radio.js stop action.
# The monitor is disabled here, proving the one-shot C recovery path works.
mkdir -p "$app/bin" "$app/scripts"
cp "$work/mixer" "$app/bin/game-mixer-harness"
${CC:-cc} -std=c11 -O2 src/radio_hotkeys.c src/game_mixer.c -o "$app/bin/radio-hotkeys"
cp scripts/background-radio.js "$app/scripts/"
cat > "$app/bin/ffplay-radio" <<'RADIO'
#!/bin/sh
exec sleep 120
RADIO
chmod +x "$app/bin/ffplay-radio"
export MT_RADIO_TEST_PLAYER=1 MUSTARDTUBE_APP_DIR="$app" MUSTARDTUBE_DATA_DIR="$app/data"
printf 'Example Station\nhttps://radio.example.org/live.mp3\n' > "$app/data/radio-selected.txt"
node "$app/scripts/background-radio.js" start >/dev/null
radiopid=$(node -p "require('$app/data/radio-background-state.json').pid")
"$work/mixer" mix-set "$app" 65
echo 0.65 > "$work/vol"
"$work/mixer" down "$app" "$radiopid"
[[ "$(cat "$work/vol")" == '0.6000' ]]
# Changing hotkeys mode from GAME to RADIO while radio is active must
# synchronously restore game volume (not rely on the monitor's 1Hz poll).
node "$app/scripts/background-radio.js" set-hotkeys 2 >/dev/null
node "$app/scripts/background-radio.js" set-hotkeys 1 >/dev/null
[[ "$(cat "$work/vol")" == '1.0000' ]]
[[ ! -e "$app/data/game-volume.session" ]]
"$work/mixer" mix-set "$app" 65
echo 0.65 > "$work/vol"
"$work/mixer" down "$app" "$radiopid"
[[ "$(cat "$work/vol")" == '0.6000' ]]
node "$app/scripts/background-radio.js" stop >/dev/null
[[ "$(cat "$work/vol")" == '1.0000' ]]
[[ ! -e "$app/data/game-volume.session" ]]
[[ ! -f "$work/unsafe" ]]
echo 'PASS: GAME slider preference, 100% reset, safety checks and radio-stop recovery'

# Beta 2.2.4 UI compatibility regression: actual controller Stop after exit.
unset MT_OVERRIDE_GAME_PID MT_RADIO_PID
# The device launcher routes RADIO_NATIVE_STOP_EXIT=87 through this controller.
node "$app/scripts/background-radio.js" set-hotkeys 0 >/dev/null
printf 'Example Station\nhttps://radio.example.org/live.mp3\n' > "$app/data/radio-selected.txt"
node "$app/scripts/background-radio.js" start >/dev/null
sleep 120 & ui_old=$!
export MT_GAME_PID="$ui_old"
"$work/mixer" mix-set "$app" 70
echo 0.70 > "$work/vol"
"$work/mixer" down "$app" 99999
[[ "$(cat "$work/vol")" == '0.6500' ]]
[[ $(wc -w < "$app/data/game-volume.session") -eq 6 ]]
kill "$ui_old" 2>/dev/null || true
wait "$ui_old" 2>/dev/null || true
# On-screen Stop is handed to background-radio.js while game is already gone.
node "$app/scripts/background-radio.js" stop >/dev/null
[[ -s "$app/data/game-volume.session" ]] || { echo 'FAIL: UI Stop discarded game record' >&2;exit 1; }
"$app/bin/radio-hotkeys" --game-session "$app" | grep -q 'awaiting_reopen'
# Different executable must NOT be given the saved game volume.
node -e 'setTimeout(() => {}, 120000)' >/dev/null & unrelated=$!
printf '%s\n' "$unrelated" > "$work/override-pid"
sleep 0.6
[[ "$(cat "$work/vol")" == '0.6500' ]]
# Launch same emulator executable under a fresh PID.
sleep 120 & ui_new=$!
printf '%s\n' "$ui_new" > "$work/override-pid"
recovered=0
for attempt in $(seq 1 32);do
  if [[ "$(cat "$work/vol")" == '1.0000' && ! -e "$app/data/game-volume.session" ]];then recovered=1;break;fi
  sleep 0.25
done
if [[ $recovered != 1 ]];then echo 'FAIL: UI Stop did not restore original after same-exe relaunch' >&2;exit 1;fi
kill "$unrelated" "$ui_new" 2>/dev/null || true
wait "$unrelated" "$ui_new" 2>/dev/null || true
rm -f "$work/override-pid"
[[ ! -f "$work/unsafe" ]]
echo 'PASS: UI Stop after exiting game + same emulator relaunch resets game to 100%' 
# Audio stream recreation while same emulator PROCESS remains alive.
sleep 120 & alive=$!
export MT_GAME_PID="$alive"
"$work/mixer" mix-set "$app" 60
echo 0.60 > "$work/vol"
"$work/mixer" down "$app" 99999
[[ "$(cat "$work/vol")" == '0.5500' ]]
export MT_RELOCATE_NODE=1
"$work/mixer" restore "$app" 99999
[[ "$(cat "$work/vol")" == '1.0000' ]]
[[ ! -e "$app/data/game-volume.session" ]]
unset MT_RELOCATE_NODE
kill "$alive" 2>/dev/null || true
wait "$alive" 2>/dev/null || true
echo 'PASS: same emulator PID can safely restore recreated PipeWire stream'

# Beta 2.2.6a regression: a PREVIOUS emulator with a different executable
# identity must not block the new game's preferred slider/shortcuts.
# Never delete the old volume-reset obligation merely to unblock a new game.
sleep 120 & old_game=$!
export MT_GAME_PID="$old_game"
unset MT_OVERRIDE_GAME_PID MT_RELOCATE_NODE MT_EXTRA_STREAM
"$work/mixer" mix-set "$app" 70
echo 0.70 > "$work/vol"
"$work/mixer" down "$app" 99999
[[ "$(cat "$work/vol")" == 0.6500 ]]
[[ -f "$app/data/game-volume.session" ]]
kill "$old_game" 2>/dev/null || true
wait "$old_game" 2>/dev/null || true
# A different executable is required; Ubuntu's Rust sleep/tail may be one
# multicall binary, hence Node is deliberately used for the new game.
node -e 'setTimeout(() => {}, 120000)' >/dev/null & other_game=$!
export MT_OVERRIDE_GAME_PID="$other_game"
"$work/mixer" mix-set "$app" 45
echo 0.90 > "$work/vol"
"$work/mixer" mix-sync "$app" 99999
[[ "$(cat "$work/vol")" == 0.4500 ]] || { echo 'FAIL: deferred old game blocked new slider' >&2; exit 1; }
[[ -f "$app/data/game-volume.deferred-0" ]]
[[ "$(awk '{print $2}' "$app/data/game-volume.session")" == "$other_game" ]]
"$work/mixer" down "$app" 99999
[[ "$(cat "$work/vol")" == 0.4000 ]] || { echo 'FAIL: new game hotkey blocked' >&2;exit 1; }
"$work/mixer" restore "$app" 99999 || true
[[ "$(cat "$work/vol")" == 1.0000 ]] || { echo 'FAIL: new game failed reset100' >&2;exit 1; }
[[ -e "$app/data/game-volume.deferred-0" ]]
# Reopening the PREVIOUS emulator is independently identity checked and
# restores to 100%, without touching unrelated PipeWire streams or sinks.
kill "$other_game" 2>/dev/null || true
wait "$other_game" 2>/dev/null || true
sleep 120 & reopened_game=$!
export MT_OVERRIDE_GAME_PID="$reopened_game"
echo 0.30 > "$work/vol"
"$work/mix-cli" --recover-once "$app"
[[ "$(cat "$work/vol")" == 1.0000 ]] || { echo 'FAIL: original deferred emulator not restored' >&2;exit 1; }
[[ ! -e "$app/data/game-volume.deferred-0" ]]
[[ ! -f "$work/unsafe" ]]
echo 'PASS: stale executable deferred safely; new mixer and hotkeys work; original game reset on relaunch'
