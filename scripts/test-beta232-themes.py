#!/usr/bin/env python3
"""Four-theme UI contract; static checks (not a device test)."""
from pathlib import Path
import re
r=Path(__file__).resolve().parent.parent
s=(r/'src/main.c').read_text(); build=(r.parent/'build-package.sh').read_text()
assert '#define TCOUNT 4' in s
assert '"midnight", "forest", "paper", "acidbloom"' in s
assert 'int x=MT_HOME_X+(i%2)*244,y=161+(i/2)*80;' in s
assert 'a->style_idx=row*2+col;' in s
assert 'MT_HOME_CARD_H 54' in s and 'MT_HOME_W 477' in s
assert 'MT_BETA "2.4.2"' in (r/'src/release.h').read_text()
for name in ('midnight','forest','paper','acidbloom'):
    data=(r/'assets/themes'/f'{name}.theme').read_text()
    assert data.count('name=')==1 and f'name={"Acid Bloom" if name=="acidbloom" else name.title()}' in data
    assert f'assets/themes/{name}.theme' in build or 'cp assets/themes/*.theme' in build
for name in ('header-dark','header-paper'):
    b=(r/'assets/branding'/f'{name}.png').read_bytes()
    assert b[:8]==b'\x89PNG\r\n\x1a\n'
    assert b[25]==6,'Header artwork must be RGBA for transparent blending'
assert 'assets/themes/acidbloom.theme:assets/themes/acidbloom.theme' in build
assert 'OTA.muxapp' in build
print('PASS: four-theme grid, packaging, header transparency and baseline cards')
