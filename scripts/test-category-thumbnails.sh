#!/usr/bin/env bash
# Fully offline, no YouTube or artwork network access.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"; for n in 10000000001 10000000002 10000000003 10000000004; do rm -f "/tmp/mustardtube-thumb-${n}.jpg"; done' EXIT
mkdir -p "$tmp/bin" "$tmp/data"
cat > "$tmp/bin/fakeytdlp" <<'MOCK'
#!/bin/sh
printf '%s\n' "$*" >> "$MUSTARDTUBE_TEST_CALLS"
[ "${MUSTARDTUBE_TEST_FAIL:-0}" = 0 ] || exit 1
printf '10000000001|||First real result|||Mock channel|||01:11\n'
printf '10000000002|||Second real result|||Mock channel|||02:22\n'
printf '10000000003|||Third real result|||Mock channel|||03:33\n'
printf '10000000004|||Fourth real result|||Mock channel|||04:44\n'
MOCK
cat > "$tmp/bin/curl" <<'MOCK'
#!/bin/sh
# Filesystem-only fake thumbnail fetch to test visible-page limits.
target=''; while [ "$#" -gt 0 ]; do case "$1" in -o)shift;target="$1";; esac; shift; done
[ -n "$target" ] || exit 2
printf 'fake JPEG only for backend page-bound test' > "$target"
MOCK
chmod +x "$tmp/bin/fakeytdlp" "$tmp/bin/curl"
export MUSTARDTUBE_APP_DIR="$tmp" MUSTARDTUBE_YTDLP="$tmp/bin/fakeytdlp" \
       MUSTARDTUBE_NODE=/bin/true MUSTARDTUBE_TEST_CALLS="$tmp/calls.txt" \
       MUSTARDTUBE_SKIP_THUMBNAILS=1
sh scripts/youtube-backend.sh home explore
sh scripts/youtube-backend.sh home music
[ "$(wc -l < "$tmp/data/home-explore-results.tsv")" -eq 4 ]
[ "$(wc -l < "$tmp/data/home-music-results.tsv")" -eq 4 ]
[ -f "$tmp/data/home-explore-updated.txt" ]
[ -f "$tmp/data/home-music-updated.txt" ]
cp "$tmp/data/home-music-results.tsv" "$tmp/musicsafe"
export MUSTARDTUBE_TEST_FAIL=1
if sh scripts/youtube-backend.sh home music; then echo 'FAIL network error not surfaced';exit 1;fi
cmp "$tmp/musicsafe" "$tmp/data/home-music-results.tsv"
unset MUSTARDTUBE_TEST_FAIL
sh scripts/youtube-backend.sh home foryou
[ ! -f "$tmp/data/home-foryou-results.tsv" ]
grep -q 'watch a video' "$tmp/data/home-foryou-status.txt"
printf '10000000001\nI watched this video\n' > "$tmp/data/selected.txt"
sh scripts/youtube-backend.sh remember
[ "$(wc -l < "$tmp/data/watch-history.tsv")" -eq 1 ]
sh scripts/youtube-backend.sh home foryou
grep -q 'ytsearch12:videos similar to I watched this video' "$tmp/calls.txt"
[ -f "$tmp/data/home-foryou-results.tsv" ]
# Four visible results fetched, with no other media or resolution operations.
printf '10000000001|||One|||C|||1:00\n10000000002|||Two|||C|||2:00\n10000000003|||Three|||C|||3:00\n10000000004|||Four|||C|||4:00\n' > "$tmp/data/search-results.tsv"
export PATH="$tmp/bin:$PATH" MUSTARDTUBE_SKIP_THUMBNAILS=0
sh scripts/youtube-backend.sh thumbs 0
for n in 10000000001 10000000002 10000000003 10000000004;do [ -s "/tmp/mustardtube-thumb-${n}.jpg" ];done
[ "$(find /tmp -maxdepth 1 -name 'mustardtube-thumb-1000000000?.jpg' | wc -l)" -eq 4 ]
sh scripts/youtube-backend.sh clear-cache
[ ! -f "$tmp/data/home-explore-results.tsv" ]
[ ! -f "$tmp/data/home-music-results.tsv" ]
[ -f "$tmp/data/watch-history.tsv" ]
sh scripts/youtube-backend.sh clear-history
[ ! -f "$tmp/data/watch-history.tsv" ]
echo 'PASS independent Home categories, offline fallback, local For You, four visible thumbnails, safe cache and history reset'
