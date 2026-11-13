#!/usr/bin/env python3
"""Beta 2.3.1 light UI asset, contrast and packaging invariants (stdlib)."""
from pathlib import Path
import struct, zlib, re, subprocess
root=Path(__file__).resolve().parent.parent
source=(root/'src/main.c').read_text()
build=(root/'build-package.sh').read_text()

def contrast(a,b):
    def lum(color):
        vals=[int(color[i:i+2],16)/255 for i in (1,3,5)]
        vals=[v/12.92 if v<=0.04045 else ((v+0.055)/1.055)**2.4 for v in vals]
        return .2126*vals[0]+.7152*vals[1]+.0722*vals[2]
    x,y=lum(a),lum(b)
    return (max(x,y)+.05)/(min(x,y)+.05)

for name in ('midnight','forest','paper','acidbloom'):
    doc={}
    for line in (root/'assets/themes'/f'{name}.theme').read_text().splitlines():
        if '=' in line and not line.lstrip().startswith('#'):
            k,v=line.split('=',1);doc[k.strip()]=v.strip()
    for k in ('background','surface','surface_selected','accent','accent_ink','accent_text','text','muted'):
        assert re.fullmatch('#[0-9A-Fa-f]{6}',doc[k]),(name,k)
    for pair in [('accent','accent_text'),('background','accent_ink'),('surface','accent_ink'),('surface_selected','accent_ink'),('surface_selected','text'),('surface','text'),('surface','muted')]:
        ratio=contrast(doc[pair[0]],doc[pair[1]])
        assert ratio>=4.5,(name,pair,round(ratio,2))
    print('PASS',name,'theme contrast')

assert 'SDL_Texture *background_texture' not in source
assert 'static void draw_background' in source
assert 'accent_ink, accent_text' in source
assert 'selected?t->accent_text:t->text' in source
assert 'sel?t->accent_text:t->text' in source
assert 'SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"linear")' in source
assert 'branding/header-paper.png' in source and 'branding/header-dark.png' in source
assert 'SDL_Texture *home_thumbs[HOME_VIDEOS]' in source
assert 'SDL_Texture *result_thumbs[RESULTS_VISIBLE]' in source
assert '#define MT_HOME_CARD_H 54' in source
assert '#define MT_HOME_W 477' in source
assert 'static void draw_footer' in source and 'MT_FOOTER,530,453' in source
assert 'MT_BETA "2.4.5"' in (root/'src/release.h').read_text()

# An RGBA muOS glyph must have transparent corners and opaque content in its
# interior; a full opaque black tile is rendered as a white square by a
# tinting theme. Parse PNG without requiring Pillow on Ubuntu.
p=root/'assets/glyph/mustardtube.png';blob=p.read_bytes()
assert blob.startswith(b'\x89PNG\r\n\x1a\n')
pos=8;data=b'';width=height=0;types=[]
while pos<len(blob):
    length=struct.unpack('>I',blob[pos:pos+4])[0]
    tag=blob[pos+4:pos+8];body=blob[pos+8:pos+8+length]
    if tag==b'IHDR':
        width,height,depth,colour,compress,filtr,inter=struct.unpack('>IIBBBBB',body)
        assert (width,height,depth,colour,inter)==(256,256,8,6,0)
    if tag==b'IDAT': data+=body
    pos+=12+length
    if tag==b'IEND':break
rows=zlib.decompress(data);size=width*4;prev=bytearray(size);alpha=[];at=0
for y in range(height):
    typ=rows[at];at+=1;cur=bytearray(rows[at:at+size]);at+=size
    for i in range(size):
        a=cur[i-4] if i>=4 else 0;b=prev[i];c=prev[i-4] if i>=4 else 0
        if typ==1: v=a
        elif typ==2:v=b
        elif typ==3:v=(a+b)//2
        elif typ==4:
            pr=a+b-c;pa=abs(pr-a);pb=abs(pr-b);pc=abs(pr-c)
            v=a if pa<=pb and pa<=pc else b if pb<=pc else c
        elif typ==0:v=0
        else:raise AssertionError('bad PNG filter')
        cur[i]=(cur[i]+v)&255
    alpha.extend(cur[3::4]);prev=cur
assert alpha[0]==0 and alpha[-1]==0
assert .03<sum(x>200 for x in alpha)/len(alpha)<.55
print('PASS muOS glyph is transparent and has visible symbol')

for name in ('header-dark','header-paper'):
    blob=(root/'assets/branding'/f'{name}.png').read_bytes()
    assert blob.startswith(b'\x89PNG\r\n\x1a\n')
    assert struct.unpack('>II',blob[16:24])==(96,96)
for item in ('midnight.theme','forest.theme','paper.theme','acidbloom.theme'):
    assert 'assets/themes/'+item in build
for item in ('header-dark.png','header-paper.png'):
    assert 'assets/branding/'+item in build
assert 'cmp -s "$source" <(unzip -p "$out" "MustardTube/$installed")' in build
assert 'OTA.muxapp' in build
# Release launcher deliberately gains OTA orchestration. Preserve only the
# multimedia controller/backend hashes in the historical baseline.
base=(root/'tests/beta231-core.sha256').read_text().splitlines()
media='\n'.join(line for line in base if 'packaging/mux_launch.sh' not in line)+'\n'
subprocess.run(['sha256sum','-c','-'],input=media,text=True,cwd=root,check=True,stdout=subprocess.DEVNULL)
print('PASS header artwork, packaged assets, preserved media/controller source')
