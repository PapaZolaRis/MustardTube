#!/usr/bin/env python3
"""Beta 2.4.3 SDL customization contract; hardware visual validation separate."""
from pathlib import Path
import re
root=Path(__file__).resolve().parent.parent
src=(root/'src/main.c').read_text()
release=(root/'src/release.h').read_text()
builder=(root/'build-package.sh').read_text()
assert '#define MT_HOME_X 145' in src and '#define MT_HOME_W 477' in src
assert '#define HOME_VIDEOS 4' in src and '#define RESULTS_VISIBLE 4' in src
assert '#define NAV_TAB_COUNT 6' in src and '#define TCOUNT 4' in src
assert 'MT_BETA "2.4.5"' in release
assert 'PERSONALIZE' in src and 'NAV_HORIZONTAL' in src and 'NAV_COMPACT' in src
assert 'a->render_content=true;' in src and 'a->render_content=false;' in src
assert 'ui_unproject_x(a,x);y=ui_unproject_y(a,y);' in src
assert 'a->card_density?6:RESULTS_VISIBLE' in src
assert 'if(p==RESULTS && !a->card_density)' in src
assert 'a->page==STYLE){' in src
assert 'else if(a->style_idx==4)set_page(a,PERSONALIZE);' in src
assert 'if(a->page==UPDATES || a->page==PERSONALIZE)' in src
assert 'if(p==PERSONALIZE)' in src and 'static void draw_personalize(App*a)' in src
assert 'a->startup_page' in src and 'static const Page start_tabs[]' in src
assert 'static bool save_settings(App *a)' in src and 'rename(tmp,path)' in src
assert 'theme=%s\\nnavigation=%d\\ncards=%d\\nstartup=%d\\ntext_size=%d' in src
assert 'if(!strcmp(line,"navigation") && value<=2)' in src
assert 'if(!strcmp(line,"cards") && value<=1)' in src
assert 'if(!strcmp(line,"startup") && value<=3)' in src
assert 'if(!strcmp(line,"text_size") && value<=2)' in src
assert 'if(!a->reset_armed)' in src and 'save_settings(a)' in src
assert 'if(a->page==PERSONALIZE)' in src
assert 'scripts/test-beta243-personalization.sh' in builder
assert 'scripts/test-beta243-personalization.py' in builder
# Media and OTA source must retain their validated version, untouched by personalization.
import hashlib,zipfile
baseline=root.parent/'MustardTube-v0.9.11-Beta2.4.2-Flat-Repo-Import.zip'
# Baseline ZIP is optional on user's VM; immutable local digest manifest is part of this source.
checks=root/'tests/beta243-protected-media.sha256'
for line in checks.read_text().splitlines():
    if not line.strip():continue
    digest,path=line.split('  ',1)
    assert hashlib.sha256((root/path).read_bytes()).hexdigest()==digest,path
print('PASS: Beta 2.4.3 3-layout navigation, 2 densities, startup, font, reset, protected media/OTA')
