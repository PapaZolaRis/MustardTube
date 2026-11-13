#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Beta 2.4.2: real progress, bounded status writer, no extra rendering runtimes.
from pathlib import Path
root=Path(__file__).resolve().parent.parent
src=(root/'src/main.c').read_text()
js=(root/'scripts/ota-update.js').read_text()
launcher=(root/'packaging/mux_launch.sh').read_text()
assert '#define MT_HOME_CARD_H 54' in src and '#define MT_HOME_W 477' in src
assert 'if(!strcmp(a->update_state,"downloading")' in src
assert 'a->update_progress>=0 && a->update_progress<=100' in src
assert 'rounded(a,x,y,completed,h' in src
assert 'progress=' in js and 'fs.renameSync(tmp, STATUS)' in js
assert "safeStatus('verifying','Verifying SHA-256 and package'" in js
assert "safeStatus('backup','Saving settings before update'" in js
assert "safeStatus('error',friendlyError(e))" in js
assert "now-lastStatus>=500 || count===asset.size" in js
assert 'a->page==UPDATES && SDL_GetTicks()-a->update_last_poll>=1000' in src
assert 'UPDATE_DOWNLOAD_EXIT 89' in src and 'task=prepare' in launcher
assert 'if(!strcmp(a->update_state,"checking") || !strcmp(a->update_state,"downloading")' in src
assert 'MT_BETA "2.4.5"' in (root/'src/release.h').read_text()
print('PASS: Beta 2.4.2 OTA percent bar, error messages, stages, bounded UI updates and unchanged media layout')
