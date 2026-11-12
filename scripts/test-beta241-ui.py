#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Static UI, installer and integration invariants for safe Beta 2.4.1 OTA.
from pathlib import Path
root=Path(__file__).resolve().parent.parent
src=(root/'src/main.c').read_text()
launcher=(root/'packaging/mux_launch.sh').read_text()
updater=(root/'scripts/ota-update.js').read_text()
build=(root.parent/'build-package.sh').read_text()
assert '#define MT_HOME_CARD_H 54' in src and '#define MT_HOME_W 477' in src
assert '#define TCOUNT 4' in src and 'SDL_Texture *home_thumbs[HOME_VIDEOS]' in src
assert 'WATCH  ·  LISTEN  ·  PLAY' in src
assert 'const char *options[]={"Check for updates"' in src
assert '"Prepare verified update"' in src
assert '"Exit for Archive Manager"' in src
assert '"Auto-check: On"' in src and '"Auto-check: Off"' in src
assert 'a->update_selected+dy+dx+12)%4' in src
assert 'if((y-250)%41<37 && row<4)' in src
assert 'case UPDATES:draw_updates(a);' in src
assert 'UPDATE_CHECK_EXIT 88' in src and 'UPDATE_DOWNLOAD_EXIT 89' in src
assert 'check-if-due' in launcher and 'task=prepare' in launcher
assert 'MUSTARDTUBE_OTA_TEST_ARCHIVE' in updater
assert 'releaseMatchesPinned' in updater and 'backupUserData' in updater
assert 'saveCandidate' in updater and 'sha256' in updater
assert 'Archive Manager performs the install' in src
assert 'cp scripts/youtube-backend.sh scripts/radio-backend.js scripts/background-radio.js scripts/ota-update.js' in build
assert '"scripts/ota-update.js:scripts/ota-update.js"' in build
assert 'SHA256SUMS.txt' in build
assert 'MT_BETA "2.4.2"' in (root/'src/release.h').read_text()
for p in ['scripts/youtube-backend.sh','scripts/radio-backend.js','scripts/background-radio.js',
          'src/game_mixer.c','assets/themes/midnight.theme','assets/themes/forest.theme',
          'assets/themes/paper.theme','assets/themes/acidbloom.theme']:
    assert (root/p).is_file()
print('PASS: Beta 2.4.1 four-theme UI, safe preparation, daily opt-in checking and packaging invariants')
