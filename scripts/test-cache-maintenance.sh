#!/usr/bin/env bash
# Test cache bounds and safe cleanup without network or device dependencies.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"; rm -f /tmp/mustardtube-thumb-CACHETEST??.jpg' EXIT
mkdir -p "$work/data" "$work/cache" "$work/downloads" "$work/safe"
export MUSTARDTUBE_APP_DIR="$work" MUSTARDTUBE_CACHE_DIR="$work/cache"
# This app has no offline downloads yet. Simulate a future library and history.
printf 'I am a saved video\n' > "$work/downloads/owned-video.mp4"
printf 'Viewing history\n' > "$work/data/watch-history.tsv"
printf '480\n' > "$work/data/quality.txt"
printf 'test query\n' > "$work/data/query.txt"
printf 'theme=forest\n' > "$work/data/settings.ini"
printf 'not our cache\n' > "$work/safe/important.txt"
# Leave stale, bogus, and oversized thumbnails from earlier app sessions.
for i in $(seq -w 0 11); do printf 'cache\n' > "/tmp/mustardtube-thumb-CACHETEST${i}.jpg"; sleep 0.01; done
python - <<'PY'
from pathlib import Path
Path('/tmp/mustardtube-thumb-CACHETEST10.jpg').write_bytes(b'X'*170000)
PY
mkdir -p "$work/data/ytdlp-cache/test"
printf 'ytcache\n' > "$work/data/ytdlp-cache/test/data"
printf 'cached response\n' > "$work/data/home-results.tsv"
mkdir -p "$work/data/radio-logo-cache"
printf 'fake logo\n' > "$work/data/radio-logo-cache/aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa.png"
printf 'saved favourite\n' > "$work/data/radio-favourites.tsv"
printf 'result\n' > "$work/data/search-results.tsv"
sh scripts/youtube-backend.sh cache-size | grep -q 'Disposable MustardTube cache:'
count=$(find /tmp -maxdepth 1 -name 'mustardtube-thumb-CACHETEST??.jpg' | wc -l)
[ "$count" -le 8 ] || { echo "FAIL: thumb count=$count" >&2; exit 1; }
[ ! -e /tmp/mustardtube-thumb-CACHETEST10.jpg ] || { echo 'FAIL oversized thumbnail not evicted' >&2; exit 1; }
sh scripts/youtube-backend.sh clear-cache
[ ! -e "$work/data/ytdlp-cache/test/data" ]
[ ! -e "$work/data/home-results.tsv" ]
[ ! -d "$work/data/radio-logo-cache" ]
[ -f "$work/data/radio-favourites.tsv" ]
[ ! -e "$work/data/search-results.tsv" ]
[ "$(find /tmp -maxdepth 1 -name 'mustardtube-thumb-CACHETEST??.jpg' | wc -l)" -eq 0 ]
[ "$(cat "$work/data/quality.txt")" = 480 ]
[ "$(cat "$work/data/query.txt")" = 'test query' ]
grep -q 'theme=forest' "$work/data/settings.ini"
grep -q 'Viewing history' "$work/data/watch-history.tsv"
grep -q 'saved video' "$work/downloads/owned-video.mp4"
grep -q 'not our cache' "$work/safe/important.txt"
grep -q 'Cache cleared' "$work/data/status.txt"
grep -q 'saved videos kept' "$work/data/status.txt"
[ "$(sh scripts/youtube-backend.sh cache-size | sed -n 's/.*cache: \([0-9]*\) KiB.*/\1/p')" -le 12 ]
printf 'PASS cache bounds, auto-prune, safe clear, preserved settings and offline user media\n'
