#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
cd "$(dirname "$0")/.."
test -s LICENSE && test -s NOTICE && test -s BRANDING.md
grep -Fq 'GNU GENERAL PUBLIC LICENSE' LICENSE
grep -Fq 'Version 3, 29 June 2007' LICENSE
grep -Fq 'END OF TERMS AND CONDITIONS' LICENSE
grep -Fq 'GPL-3.0-only' src/release.h src/main.c README.md
! grep -Eiq 'MustardTube source: MIT licen[sc]e' src/main.c
! grep -Eiq 'project source code is distributed under the \[MIT' README.md
for name in assets/grid/mustardtube.png assets/glyph/mustardtube.png; do
  test -s "$name" && file "$name" | grep -Fq 'PNG image'
done
python3 - <<'PYCODE'
from pathlib import Path
from struct import unpack
for name,side in [('assets/grid/mustardtube.png',160),('assets/glyph/mustardtube.png',256)]:
    b=Path(name).read_bytes()
    assert b[:8]==b'\x89PNG\r\n\x1a\n'
    assert unpack('>II',b[16:24])==(side,side)
print('PASS logo dimensions and format')
PYCODE
bash -n ../build-package.sh
bash -n scripts/youtube-backend.sh
bash -n packaging/mux_launch.sh
for package_script in scripts/package-*.sh; do bash -n "$package_script"; done
node --check scripts/radio-backend.js
node --check scripts/background-radio.js
echo 'PASS GPL-3.0-only first-party declarations, official license, logos, packaging syntax'
