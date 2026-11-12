#!/usr/bin/env bash
# Current-release UI/source assertions. This replaces the obsolete 2.2.3b-only test.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PYTEST'
from pathlib import Path
s=Path('src/main.c').read_text()
pack=Path('scripts/package-beta22-from-beta21-installer.sh').read_text()
ctl=Path('scripts/background-radio.js').read_text()
game=Path('src/game_mixer.c').read_text()
assert 'MT_FOOTER,530,453' in s, 'footer layout/version marker missing'
assert '"Version " MT_RELEASE' in s
assert 'Radio Volume  %d%%' in s and 'Game Volume  %d%%' in s
assert 'a->radio_now_slider?game_mix_adjust(a,1):radio_mix_adjust(a,1)' in s
assert 'a->radio_now_slider?game_mix_adjust(a,-1):radio_mix_adjust(a,-1)' in s
assert 'Radio Volume  %d%%' in s and 'Game Volume  %d%%' in s, 'radio mix labels lost'
assert 'RADIO+GAME' in s and 'radio_hotkeys_toggle(a)' in s
assert '#define SEARCH_RESULTS 120' in s
assert '#define HOME_VIDEOS 4' in s
assert 'RADIO_MAX_STATIONS' not in s and 'RadioStation *stations;' in s
assert 'IN-GAME HOTKEYS' in s and 'INDEPENDENT RADIO VOLUME' not in s
assert 'radio_mix_edit' not in s
assert 'Beta2.2.6a-DualMixer-StaleSessionFix' in pack
assert 'single-press and hold-to-repeat' in pack
assert '--set-game-mix' in s and 'get-hotkeys' in ctl
assert 'mt_game_mix_sync' in game and 'mt_game_restore' in game
ignore=Path('.gitignore').read_text().splitlines()
assert '*.png' not in [line.strip() for line in ignore], 'source artwork must not be globally ignored'
print('PASS: Current UI version, sliders, shortcuts, player spacing, source packaging and asset tracking')
PYTEST
