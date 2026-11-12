#!/usr/bin/env python3
# GPL-3.0-only. Beta 2.4.0 menu and package smoke invariants.
from pathlib import Path
root=Path(__file__).resolve().parent.parent
src=(root/'src/main.c').read_text()
launcher=(root/'packaging/mux_launch.sh').read_text()
build=(root/'build-package.sh').read_text()
assert '#define MT_HOME_CARD_H 54' in src and '#define MT_HOME_W 477' in src
assert '#define TCOUNT 4' in src and 'SDL_Texture *home_thumbs[HOME_VIDEOS]' in src
assert 'WATCH  ·  LISTEN  ·  PLAY' in src
assert 'case UPDATES:draw_updates(a);' in src
assert '--show-updates' in src and '--show-updates' in launcher
assert 'UPDATE_CHECK_EXIT 88' in src and 'UPDATE_DOWNLOAD_EXIT 89' in src
assert '88|89)' in launcher and 'ota-update.js' in launcher
assert 'a->page==UPDATES && SDL_GetTicks()-a->update_last_poll>=1000' in src
assert 'SDL_CONTROLLER_BUTTON_BACK:' in src
assert 'if(a->page==STYLE)open_updates(a);' in src
assert 'cp scripts/youtube-backend.sh scripts/radio-backend.js scripts/background-radio.js scripts/ota-update.js' in build
assert '"scripts/ota-update.js:scripts/ota-update.js"' in build
assert 'OTA.muxapp' in build and 'SHA256SUMS.txt' in build
assert 'MT_BETA "2.4.2"' in (root/'src/release.h').read_text()
for p in ['scripts/youtube-backend.sh','scripts/radio-backend.js','scripts/background-radio.js','src/game_mixer.c']:
 assert Path(root/p).is_file()
print('PASS: Beta 2.4.0 UI, exit handoff, package and media-source invariants')
