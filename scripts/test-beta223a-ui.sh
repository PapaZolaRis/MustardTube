#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PYTEST'
from pathlib import Path
s=Path('src/main.c').read_text()
assert s.count('static bool radio_mix_adjust(App*a,int direction)')==1
start=s.index('static bool radio_mix_adjust(App*a,int direction)')
block=s[start:s.index('/* Signed URLs never enter SDL.',start)]
assert block.index('radio_mix_load(a);') < block.index('int level=a->radio_mix_volume+direction*5;')
start=s.index('if((Uint32)(tick-a->radio_now_last_tick)>=1000)')
block=s[start:start+550]
assert 'radio_mix_load(a);' in block and 'a->dirty=true' in block
assert '"v0.9.11b2.2.3b"' in s
assert 'Beta 2.2.3b HoldRepeat UI Sync Game Restore' in s
assert 'Beta 2.2.1 EXP' not in s
assert 'Beta 2.2.2 navigation hotfix' not in s
pkg=Path('scripts/package-beta22-from-beta21-installer.sh').read_text()
assert 'Beta2.2.3b-HoldRepeat-UI-Sync-GameRestore-EXPERIMENTAL' in pkg
assert 'single-press and hold-to-repeat' in pkg
print('PASS: UI refresh, authoritative radio volume step, version identifiers, and package helper guard')
PYTEST
