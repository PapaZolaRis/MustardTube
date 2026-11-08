#!/usr/bin/env bash
# Source-only preflight; not a substitute for ARM64 compilation and real-device audio.
set -euo pipefail
cd "$(dirname "$0")/.."
node --check scripts/background-radio.js
sh -n packaging/mux_launch.sh
bash -n scripts/build-background-player-arm64.sh
bash -n scripts/package-beta22-from-beta21-installer.sh
bash -n scripts/audit-player-arm64.sh
bash scripts/test-background-radio.sh
bash scripts/test-radio-mix-volume.sh
bash scripts/test-radio-ingame-hotkeys.sh
bash scripts/test-game-volume.sh
bash scripts/test-background-launcher.sh
bash scripts/test-background-video-handoff.sh
bash scripts/test-radio-launcher.sh
bash scripts/test-native-radio-launcher.sh
bash scripts/test-beta221-resume.sh
bash scripts/test-radio-tab-navigation.sh
node scripts/test-radio-backend.js
bash scripts/test-youtube-backend.sh
bash scripts/test-cache-maintenance.sh
bash scripts/test-category-thumbnails.sh
bash scripts/test-display-compat.sh
python3 scripts/test-radio-visual-assets.py
python3 - <<'PY'
from pathlib import Path
src=Path('src/main.c').read_text()
launcher=Path('packaging/mux_launch.sh').read_text()
for name in ('RADIO_BACKGROUND_START_EXIT 84','RADIO_BACKGROUND_STOP_EXIT 85',
             'RADIO_NATIVE_START_EXIT 86','RADIO_NATIVE_STOP_EXIT 87',
             'draw_radio_now(App*a)','radio_start_native_station(App*a',
             'radio_start_background(App*a)', 'radio_stop_background(App*a)',
             'SDL_CONTROLLER_BUTTON_BACK','SDL_CONTROLLER_BUTTON_START',
             'BG RADIO: %s', 'radio-background-name.txt'):
    assert name in src, name
for name in ('84)', '85)', '86)', '87)', '--show-radio-now',
             '--live --title "$title" "$playlist"',
             '--live --title "$title" "$stream"', 'RADIO_CTL=', 'radio-now-playing.m3u'):
    assert name in launcher, name
print('PASS: native radio routes, independent playback, and unchanged Wasabi fallback present')
PY

python3 - <<'PY_NATIVE_RADIO_BETA221'
from pathlib import Path
src=Path('src/main.c').read_text()
assert 'radio_now_blink' in src
assert 'radio_now_logo_checked' in src
assert 'a->radio_now_blink=!a->radio_now_blink;' in src
assert 'SDL_WaitEventTimeout(&e,1000)' in src
assert 'RADIO ACTIVE' in src
assert 'X: In-game OFF' in src
assert 'X: In-game RADIO+GAME' in src
launcher=Path('packaging/mux_launch.sh').read_text()
assert 'if [ -z "$resume" ] && [ -s "$APP_DIR/data/radio-background-name.txt" ]' in launcher
ctl=Path('scripts/background-radio.js').read_text()
assert "if(process.argv[3]==='1'||process.argv[3]==='2'){" in ctl
print('PASS: 1Hz radio status dot, cached artwork, resume to live player and helper reattach')
PY_NATIVE_RADIO_BETA221
