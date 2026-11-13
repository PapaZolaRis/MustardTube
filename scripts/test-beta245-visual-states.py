#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Beta 2.4.5 rendering contract: loading != empty != actionable play.
Source regression only; inspect real pixels on Plus/SP before distributing.
"""
import hashlib
from pathlib import Path
import re
root=Path(__file__).resolve().parent.parent
src=(root/'src/main.c').read_text()
release=(root/'src/release.h').read_text()
updater=(root/'scripts/ota-update.js').read_text()
assert 'MT_BETA "2.4.5"' in release
assert "const CURRENT = 'v0.9.11-beta2.4.5';" in updater

def fn(name):
    m=re.search(r'static void '+re.escape(name)+r'\([^)]*\)\s*\{',src)
    assert m,name
    start=m.end()-1;depth=0
    for j in range(start,len(src)):
        if src[j]=='{':depth+=1
        elif src[j]=='}':
            depth-=1
            if depth==0:return src[start:j+1]
    raise AssertionError(f'function did not close: {name}')

helper=fn('video_placeholder')
assert 'outline(a,x,y,w,h' in helper
assert 'large?60:34' in helper and 'large?42:26' in helper
assert 'SDL_Texture' not in helper and 'SDL_Create' not in helper
loading=fn('loading_dots')
assert 'SDL_GetTicks()/420u' in loading
assert 'a->theme.accent' in loading and 'a->theme.muted' in loading
assert 'circle(' in loading
assert 'triangle(a,' not in src, 'legacy generic play triangles must be gone'
assert 'text_center(a,FS_19,"MT",x,y+(size-19)/2,size' in fn('draw_brand_icon')

for name in ('home_video_card','draw_home','draw_results','draw_player'):
    body=fn(name)
    assert 'video_placeholder(a,' in body,name
    # Old solid yellow play badge must only remain on genuinely playable video previews.
    assert 'triangle(a,' not in body,name
home=fn('draw_home')
assert 'if(a->home_loading)loading_dots(' in home
assert 'else video_placeholder(' in home
assert 'Loading videos...' in home and 'No videos yet' in home
results=fn('draw_results')
assert 'No videos returned' in results
assert results.count('video_placeholder(')==2
player=fn('draw_player')
assert 'preview_play_symbol(' in player
assert 'if(a->current_is_youtube && a->thumbnail)' in player
assert 'A  PLAY' in player
assert 'video_placeholder(a,' in player
radio=fn('draw_radio')
assert 'if(a->radio_loading)loading_dots(' in radio
assert 'No stations. Press X to retry.' in radio
# No idle redrawing for loading animation; bounding to relevant page and state.
assert 'a->page==HOME && a->home_count==0 && a->home_loading' in src
assert 'a->page==RADIO && a->radio_loading &&' in src
assert 'a->radio_mode==0?a->country_count==0:' in src
assert 'a->radio_mode==2?a->favourite_count==0:a->station_count==0' in src
assert '(Uint32)(SDL_GetTicks()-a->loading_dot_last_tick)>=420u' in src
assert 'SDL_CreateTexture' not in loading and 'SDL_CreateTexture' not in helper
# 640x480 guardrails incl. projected horizontal mode bounds.
for cy in (239,235,212):
    y0=cy-42//2;y1=cy+42//2
    assert 174<=y0 and y1<320
    projected_y0=35+y0*9//10
    projected_y1=35+y1*9//10
    assert projected_y0>=35 and projected_y1<437
# Playback/backend/radio controller source still exactly matches the protected baseline.
protected=(root/'tests/beta243-protected-media.sha256').read_text().splitlines()
for entry in protected:
    if not entry.strip():continue
    digest,path=entry.split('  ',1)
    assert hashlib.sha256((root/path).read_bytes()).hexdigest()==digest,path
print('PASS: Beta 2.4.5 distinct Home/Results/Radio loading & empty states, video preview, version, and protected media')
