#!/usr/bin/env python3
"""Offline check for country sprite atlas without third-party Python packages."""
import re,struct
from pathlib import Path
root=Path(__file__).resolve().parent.parent
asset=root/'assets/flags/country-atlas.png'
raw=asset.read_bytes()
assert raw[:8]==b'\x89PNG\r\n\x1a\n'
w,h=struct.unpack('>II',raw[16:24]);assert (w,h)==(512,368),(w,h)
assert len(raw)<180_000,len(raw)
codes=(root/'assets/flags/codes.txt').read_text().strip()
assert len(codes)==498 and len(set(codes[i:i+2] for i in range(0,len(codes),2)))==249
source=(root/'src/main.c').read_text()
m=re.search(r'RADIO_FLAG_CODES="([A-Z]+)";',source)
assert m and m.group(1)==codes
assert 'CC BY 4.0' in (root/'assets/flags/ATTRIBUTION.txt').read_text()
print('PASS: 249 country flags, 512x368 atlas (<180 KB), C mapping, CC BY attribution')
