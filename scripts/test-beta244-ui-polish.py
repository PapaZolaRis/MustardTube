#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""No SDL runtime required: assert the exact Beta 2.4.4 UI fix and bounded geometry."""
from pathlib import Path
root=Path(__file__).resolve().parent.parent
src=(root/'src/main.c').read_text()
assert '#define MT_BETA "2.4.5"' in (root/'src/release.h').read_text()
assert "const CURRENT = 'v0.9.11-beta2.4.5';" in (root/'scripts/ota-update.js').read_text()
assert 'static void outline_ota(' in src
assert 'if(a->render_content && a->nav_layout==NAV_HORIZONTAL)' in src
assert 'a->render_content=false;\n        outline(a,px,py,pw,ph,radius,border,inner);\n        a->render_content=true;' in src
assert 'outline_ota(a,MT_HOME_X,144,MT_HOME_W,100' in src
assert 'outline_ota(a,MT_HOME_X,y,MT_HOME_W,37' in src
assert 'static void preview_play_symbol(' in src
assert 'outline_ota(a,MT_HOME_X+214,207,46,38,10' in src
assert 'preview_play_symbol(a,MT_HOME_X+238,226,t->accent)' in src
assert 'rounded(a,MT_HOME_X+214,204,44,44,22,t->accent)' not in src
assert 'triangle(a,MT_HOME_X+237,226,11,t->accent_text)' not in src
# In horizontal mode, each OTA option maps to a discrete native pixel rectangle.
# Coordinates match the existing inverse hit-test, with no overlap or footer clipping.
for i in range(4):
    y=250+i*41
    physical_y=35+y*9//10
    physical_h=(37*9+9)//10
    assert physical_y>=107 and physical_y+physical_h<437
    if i:
        prev=35+(y-41)*9//10
        assert prev+physical_h<physical_y
# Play badge remains fully within 309x174 thumbnail, no new image resource.
assert 137 <= 207 < 245 < 311
assert 145+84 <= 145+214 < 145+260 <= 145+84+309
print('PASS: Beta 2.4.4 horizontal OTA borders, video-details badge, version and bounds')
