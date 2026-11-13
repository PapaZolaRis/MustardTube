#!/usr/bin/env python3
"""Beta 2.3.0 static SDL2 layout/input contract (not a device/video test)."""
from pathlib import Path
import re
root=Path(__file__).resolve().parent.parent
src=(root/'src/main.c').read_text()

def c(name):
    m=re.search(r'^#define '+re.escape(name)+r' (\d+)$',src,re.M)
    assert m,name
    return int(m.group(1))
assert c('W')==640 and c('H')==480
assert c('HOME_VIDEOS')==4 and c('RESULTS_VISIBLE')==4
assert c('NAV_TAB_COUNT')==6
assert c('MT_SIDE_X')+c('MT_SIDE_W')<c('MT_HOME_X')
assert c('MT_HOME_X')+c('MT_HOME_W')<=c('W')-8
assert c('MT_HOME_X')+5*80+c('MT_HOME_CATEGORY_W')<=c('W')-8
assert c('MT_HOME_CARD_Y')+3*c('MT_HOME_CARD_STEP')+c('MT_HOME_CARD_H')<=437
assert c('MT_SIDE_ROW_Y')+5*c('MT_SIDE_ROW_STEP')+38<437
assert c('MT_CARD_THUMB_W')<=c('MT_HOME_W')//3
assert c('MT_CARD_THUMB_H')<c('MT_HOME_CARD_H')
assert abs(c('MT_CARD_THUMB_W')*9-c('MT_CARD_THUMB_H')*16)<16, 'thumbnails should be ~16:9'
assert c('RADIO_LIST_Y')+c('RADIO_VISIBLE')*c('RADIO_ROW_H')<437
# Verify uniform cards, including thumbnail dimensions, in both views.
def function(name):
    m=re.search(r'static void '+name+r'\([^)]*\)\s*\{',src)
    assert m,name
    st=m.end()-1; depth=0
    for j in range(st,len(src)):
        if src[j]=='{':depth+=1
        if src[j]=='}':
            depth-=1
            if depth==0:return src[st:j+1]
    raise AssertionError(name)
home=function('home_video_card')
results=function('draw_results')
assert 'MT_HOME_CARD_H' in home and 'MT_HOME_CARD_H' in results
assert 'MT_CARD_THUMB_W,th=MT_CARD_THUMB_H' in home
assert 'MT_CARD_THUMB_W,MT_CARD_THUMB_H' in results
assert 'MT_HOME_FIRST_H' not in src and 'MT_HOME_OTHER_H' not in src
assert 'i==0?' not in home, 'featured card still exists'
assert 'MT_HOME_CARD_STEP' in home and 'MT_HOME_CARD_STEP' in function('mouse_down')
# Shared sidebar should be rendered once for every page.
render=function('render')
assert 'draw_header(a);draw_home_sidebar(a);' in render
for page in ('HOME','SEARCH','SAVED','RADIO','RADIO_NOW','STYLE','ABOUT','PLAYER','RESULTS'):
    assert 'case '+page+':' in render
sidebar=function('draw_home_sidebar')
assert 'if(a->page==RESULTS)active=SEARCH;' in sidebar
assert 'if(a->page==RADIO_NOW)active=RADIO;' in sidebar
mouse=function('mouse_down')
assert 'x>=MT_SIDE_X+6' in mouse
assert 'open_nav_tab(a,(Page)tab)' in mouse
for kind in ('RADIO','STYLE','SEARCH','SAVED','RESULTS'):
    assert 'a->page=='+kind in mouse
# Legacy controls must still be available (no new input mode).
assert 'SDL_CONTROLLER_BUTTON_LEFTSHOULDER:cycle_page(a,-1)' in src
assert 'SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:cycle_page(a,1)' in src
assert 'SDL_CONTROLLER_BUTTON_A:activate(a)' in src
assert 'SDL_CONTROLLER_BUTTON_B:back(a)' in src
# Memory and logo constraints.
assert 'SDL_Texture *home_thumbs[HOME_VIDEOS]' in src
assert 'SDL_Texture *result_thumbs[RESULTS_VISIBLE]' in src
assert 'a->brand_icon_checked=true;' in src
assert '"branding/header-dark.png"' in src
assert '"branding/header-paper.png"' in src
assert 'SDL_RenderSetLogicalSize(a->r,W,H)' in src
# Packaging paths must agree with runtime paths.
builder=(root/'build-package.sh').read_text()
for path in ('glyph/mustardtube.png','grid/mustardtube.png','assets/themes/midnight.theme'):
    assert path in builder
assert 'cmp -s "$source" <(unzip -p "$out" "MustardTube/$installed")' in builder
print('PASS: unified sidebar, equal-size video cards, 640x480 bounds, input/asset contracts')
