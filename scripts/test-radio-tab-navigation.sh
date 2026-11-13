#!/usr/bin/env bash
# Lightweight native regression for the ACTUAL radio tab functions extracted
# from main.c, not the full SDL app. No cross-architecture pkg-config or SDL.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
python3 - <<'PY'
from pathlib import Path
import re
s=Path('src/main.c').read_text()

def function(name):
    pat=re.compile(r'\bstatic\s+void\s+'+re.escape(name)+r'\s*\([^)]*\)\s*\{')
    m=pat.search(s)
    assert m, f'Expected navigation function not found: {name}'
    depth=0
    for i in range(s.index('{',m.start()),len(s)):
        if s[i]=='{':depth+=1
        elif s[i]=='}':
            depth-=1
            if depth==0:return s[m.start():i+1]
    raise AssertionError(f'Unterminated function: {name}')

# Check the actual radio-live check, page opening and routing remain wired up.
assert 'static bool radio_now_process_alive(App*a)' in s
assert 'realpath(expected,resolved)' in s and 'strcmp(exe,resolved)==0' in s
assert 'if(p==RADIO_NOW)radio_now_load(a);' in s
assert 'else if(a->page==RADIO_NOW)set_page(a,RADIO);' in s
assert 'case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:cycle_page(a,-1)' in s
assert 'case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:cycle_page(a,1)' in s
assert 'case SDLK_F4:open_nav_tab(a,RADIO)' in s
assert 'open_nav_tab(a,(Page)tab)' in s, 'sidebar tab routing absent'

prefix=r'''#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define NAV_TAB_COUNT 6
typedef enum { HOME,SEARCH,SAVED,RADIO,STYLE,ABOUT,PLAYER,RESULTS,RADIO_NOW,UPDATES,EXTRA,PERSONALIZE } Page;
typedef struct { Page page,from_page; char status_message[176]; bool radio_active; int directory_visits; } App;
static bool radio_now_process_alive(App*a){return a->radio_active;}
static void set_page(App*a,Page p){ a->page=p; if(p==RADIO)a->directory_visits++; }
'''
post=r'''
int main(void){
    App a={0};a.radio_active=true;a.page=SAVED;
    cycle_page(&a,1);assert(a.page==RADIO_NOW&&a.directory_visits==0);
    cycle_page(&a,1);assert(a.page==STYLE&&a.directory_visits==0);
    cycle_page(&a,-1);assert(a.page==RADIO_NOW&&a.directory_visits==0);
    cycle_page(&a,-1);assert(a.page==SAVED&&a.directory_visits==0);
    open_nav_tab(&a,RADIO);assert(a.page==RADIO_NOW&&a.directory_visits==0);
    set_page(&a,RADIO);assert(a.page==RADIO&&a.directory_visits==1);/* B: intentional directory entry */
    open_nav_tab(&a,RADIO);assert(a.page==RADIO_NOW&&a.directory_visits==1);
    a.page=RADIO_NOW;cycle_page(&a,-1);assert(a.page==SAVED);
    a.radio_active=false;cycle_page(&a,1);assert(a.page==RADIO&&a.directory_visits==2);
    a.page=STYLE;cycle_page(&a,-1);assert(a.page==RADIO&&a.directory_visits==3);
    a.radio_active=true;a.page=PLAYER;a.from_page=SEARCH;
    cycle_page(&a,1);assert(a.page==SAVED&&a.directory_visits==3);
    a.page=UPDATES;cycle_page(&a,-1);assert(a.page==RADIO_NOW&&a.directory_visits==3);
    a.page=EXTRA;cycle_page(&a,-1);assert(a.page==STYLE&&a.directory_visits==3);
    puts("PASS: extracted native C L1/R1 Radio tab logic, active/stopped cases, directory escape and routing");
    return 0;
}
'''
Path('build/radio-tab-navigation-regression.c').write_text(prefix+'\n'+function('open_nav_tab')+'\n'+function('cycle_page')+'\n'+post)
PY
cc -std=c11 -O2 -Wall -Wextra -Werror build/radio-tab-navigation-regression.c -o build/radio-tab-navigation-regression
./build/radio-tab-navigation-regression
