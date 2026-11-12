#!/usr/bin/env bash
# Network-free integration tests using the actual backend and controlled extraction.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/bin" "$tmp/data"
cat > "$tmp/bin/yt-dlp" <<'MOCK'
#!/bin/sh
printf '%s\n' "$*" >> "$MOCK_CALLS"
# yt-dlp writes live_status to a private file during the SAME extraction;
# no second YouTube request is needed to identify live versus VOD HLS.
meta_file=''; previous=''
for item do
  case "$previous" in
    --print-to-file) previous=template; continue;;
    template) meta_file="$item"; previous=''; continue;;
  esac
  if [ "$item" = '--print-to-file' ]; then previous='--print-to-file'; fi
done
for arg do
 case "$arg" in
  ytsearch*:*)
    end=${arg#ytsearch};end=${end%%:*}
    case "$arg" in *':science space'|*':popular videos today') ;; *) exit 2;; esac
    [ "${MOCK_FAIL:-0}" = 0 ] || exit 2
    start=1;prev=''
    for item do
      if [ "$prev" = '--playlist-start' ];then start="$item";break;fi
      prev="$item"
    done
    n=$end
    [ "${MOCK_SHORT:-0}" = 0 ] || n=18
    awk -v start="$start" -v n="$n" -v dup="${MOCK_DUP:-0}" 'BEGIN {for(i=start;i<=n;i++){id=i;if(dup && i>=12 && i<=16)id=i-8;printf "%011d|||Science video %d|||Test Channel|||02:15\n",id,id}}' 
    exit 0;;
  https://www.youtube.com/watch?v=LIVE0000001)
    [ -z "$meta_file" ] || printf 'is_live\n' > "$meta_file"
    # Live video supplies separate video/audio URLs. HD 720p must be selected
    # explicitly and must never be silently mislabeled as a lower resolution.
    case "$*" in
      *'[height=1080]'*)
        [ "${MOCK_NO_HD:-0}" = 0 ] || exit 2
        printf 'https://cdn.example.invalid/video1080.m3u8\n'
        if [ "${MOCK_SINGLE_LIVE:-0}" = 1 ]; then exit 0; fi
        :
        ;;
      *'[height=720]'*)
        [ "${MOCK_NO_HD:-0}" = 0 ] || exit 2
        printf 'https://cdn.example.invalid/video720.m3u8?expire=9999999999\n';;
      *) printf 'https://cdn.example.invalid/video360.m3u8?expire=9999999999\n';;
    esac
    printf 'https://cdn.example.invalid/audio234.m3u8?expire=9999999999\n'
    exit 0;;
  https://www.youtube.com/watch?v=*)
    [ -z "$meta_file" ] || printf 'not_live\n' > "$meta_file"
    [ "${MOCK_HANG:-0}" = 0 ] || sleep 5
    [ "${MOCK_VIDEO_FAIL:-0}" = 0 ] || exit 2
    case "$*" in
      *'[height=1080]'*)
        [ "${MOCK_NO_HD:-0}" = 0 ] || exit 2
        printf 'https://cdn.example.invalid/video1080.m3u8\n'
        if [ "${MOCK_SINGLE_LIVE:-0}" = 1 ]; then exit 0; fi
        printf 'https://cdn.example.invalid/audio.m3u8\n'
        ;;
      *'[height=720]'*)
        [ "${MOCK_NO_HD:-0}" = 0 ] || exit 2
        printf 'https://cdn.example.invalid/vod720.m3u8?expire=9999999999\n'
        printf 'https://cdn.example.invalid/vod-audio.m3u8?expire=9999999999\n';;
      *) echo "https://cdn.example.invalid/video.mp4?token=abc&expire=${MOCK_EXPIRY:-9999999999}";;
    esac
    exit 0;;
 esac
done
exit 2
MOCK
chmod +x "$tmp/bin/yt-dlp"
printf '#!/bin/sh\nexit 0\n' > "$tmp/bin/qjs"; chmod +x "$tmp/bin/qjs"
export MUSTARDTUBE_APP_DIR="$tmp" MUSTARDTUBE_CACHE_DIR="$tmp/cache"
export MOCK_CALLS="$tmp/calls.txt" MUSTARDTUBE_SKIP_THUMBNAILS=1
backend() { sh scripts/youtube-backend.sh "$@"; }
new_search() { echo 'science space' > "$tmp/data/pending-search.txt"; backend search; }
count() { wc -l < "$tmp/data/search-results.tsv"; }
# Home discovery uses the same real YouTube metadata format as Search,
# but must never overwrite a user's Search results or query.
backend home
[ "$(wc -l < "$tmp/data/home-results.tsv")" -eq 4 ]
[ ! -e "$tmp/data/search-results.tsv" ]
grep -q '^00000000001|||Science video 1|||Test Channel|||02:15$' "$tmp/data/home-results.tsv"
cp "$tmp/data/home-results.tsv" "$tmp/home-before"
# Next two refreshes consume the 12-item cached pool without extra extraction.
: > "$MOCK_CALLS"
backend home; backend home
[ "$(wc -l < "$MOCK_CALLS")" -eq 0 ]
[ "$(wc -l < "$tmp/data/home-results.tsv")" -eq 4 ]
export MOCK_FAIL=1
if backend home; then echo 'FAIL: Home refresh should signal failure' >&2; exit 1; fi
cmp "$tmp/data/home-results.tsv" "$tmp/data/home-explore-results.tsv"
unset MOCK_FAIL
backend home
[ "$(wc -l < "$tmp/data/home-results.tsv")" -eq 4 ]
# Home must be able to show more than 100 *distinct* videos across X refreshes,
# while never writing more than four visible cards at a time.
for i in $(seq 1 27); do backend home; [ "$(wc -l < "$tmp/data/home-results.tsv")" -le 4 ]; done
[ "$(wc -l < "$tmp/data/home-explore-pool.tsv")" -ge 112 ]
[ "$(cut -d '|' -f1 "$tmp/data/home-explore-pool.tsv" | sort -u | wc -l)" -ge 112 ]
[ "$(wc -l < "$tmp/data/home-results.tsv")" -eq 4 ]
echo 'PASS: 4 visible Home cards, >100 unique refresh results, incremental metadata, offline fallback'
: > "$MOCK_CALLS"
new_search
[ "$(count)" -eq 8 ]
new_search
[ "$(wc -l < "$MOCK_CALLS")" -eq 1 ]
# Duplicate-bearing second batch used to disable X permanently.
export MOCK_DUP=1
backend more
[ "$(count)" -eq 11 ]
[ "$(cat "$tmp/data/more-available.txt")" = 1 ]
backend more
[ "$(count)" -eq 19 ]
[ "$(cat "$tmp/data/search-cursor.txt")" = 24 ]
[ "$(cat "$tmp/data/next-result-index.txt")" = 11 ]
for i in $(seq 1 20); do
  [ "$(cat "$tmp/data/more-available.txt")" = 1 ] || break
  backend more
done
[ "$(count)" -eq 120 ]
[ "$(cat "$tmp/data/more-available.txt")" = 0 ]
[ "$(cut -d '|' -f1 "$tmp/data/search-results.tsv" | sort -u | wc -l)" -eq 120 ]
echo 'PASS: repeated X, duplicate batches, selection and 120-result limit'
# Failure must preserve list, cursor and retry availability.
rm "$tmp/data/cached-search-time.txt"
unset MOCK_DUP
new_search
cp "$tmp/data/search-results.tsv" "$tmp/before"
export MOCK_FAIL=1
if backend more; then exit 1; fi
cmp "$tmp/before" "$tmp/data/search-results.tsv"
[ "$(cat "$tmp/data/search-cursor.txt")" = 8 ]
[ "$(cat "$tmp/data/more-available.txt")" = 1 ]
unset MOCK_FAIL
backend more
export MOCK_SHORT=1
backend more
[ "$(count)" -eq 18 ]
[ "$(cat "$tmp/data/more-available.txt")" = 0 ]
unset MOCK_SHORT
echo 'PASS: network failure retry and true end of results'
printf '00000000001\nTest\n' > "$tmp/data/selected.txt"
backend prefetch
calls=$(wc -l < "$MOCK_CALLS")
backend resolve
[ "$(wc -l < "$MOCK_CALLS")" -eq "$calls" ]
grep -q '^360$' "$tmp/data/prefetch-ready.txt"
echo 480 > "$tmp/data/quality.txt"
backend prefetch
[ "$(wc -l < "$MOCK_CALLS")" -eq "$((calls+1))" ]
grep -q '^480$' "$tmp/data/prefetch-ready.txt"
backend invalidate
[ ! -f "$tmp/data/prefetch-ready.txt" ]
backend resolve
[ "$(wc -l < "$MOCK_CALLS")" -eq "$((calls+2))" ]
# Verify one-step fallback format selection is present and extraction retries are enabled.
grep -Fq 'best[ext=mp4][acodec!=none]' "$MOCK_CALLS"
grep -Fq -- '--extractor-retries 1 --retries 1' "$MOCK_CALLS"
echo 'PASS: playback format fallback and bounded extraction retry'
echo 'PASS: prepared playback reuse, quality separation and invalidation'
backend invalidate
export MOCK_EXPIRY=1
if backend resolve; then exit 1; fi
unset MOCK_EXPIRY
export MOCK_HANG=1 MUSTARDTUBE_RESOLVE_TIMEOUT=1
start=$SECONDS
if backend prefetch; then exit 1; fi
[ "$((SECONDS-start))" -lt 4 ]
grep -q '^00000000001$' "$tmp/data/prefetch-failed.txt"
unset MOCK_HANG MUSTARDTUBE_RESOLVE_TIMEOUT
backend prefetch
[ ! -f "$tmp/data/prefetch-failed.txt" ]
echo 'PASS: expired URL rejection, bounded timeout, retry recovery'
printf '0000000000;\nBad\n' > "$tmp/data/selected.txt"
if backend resolve; then exit 1; fi
printf 'unknown\n' > "$tmp/data/pending-search.txt"
if backend search; then exit 1; fi
[ ! -f "$tmp/data/search-results.tsv" ]
if grep -r 'token=abc' "$tmp/data/"*.log; then echo 'URL leaked into logs';exit 1;fi
echo 'PASS: invalid input, stale-result removal, sanitized logs'
# A failed live extraction must surface a safe, actionable category rather than
# overwrite status with an unhelpful generic message.
rm -f "$tmp/data/cached-search-time.txt"
export MOCK_FAIL=1
printf 'science space\n' > "$tmp/data/pending-search.txt"
if backend search; then exit 1; fi
grep -Eq 'Search failed|YouTube search failed' "$tmp/data/status.txt"
grep -q 'search_failure category=extractor' "$tmp/data/timing.log"
unset MOCK_FAIL
echo 'PASS: failed searches retain safe diagnostic category'
# Clearing cache preserves preferences, query and saved settings.
echo 'science space' > "$tmp/data/query.txt"
echo 'theme=forest' > "$tmp/data/settings.ini"
printf 'fav-id|||Radio Test|||MP3|||https://example.org/live|||US\n' > "$tmp/data/radio-favourites.tsv"
printf 'temporary country cache\n' > "$tmp/data/radio-countries.tsv"
printf 'temporary station cache\n' > "$tmp/data/radio-stations.tsv"
mkdir -p "$tmp/data/ytdlp-cache/test"
echo cached > "$tmp/data/ytdlp-cache/test/data"
backend clear-cache
[ ! -e "$tmp/data/ytdlp-cache/test/data" ]
[ ! -e "$tmp/data/search-results.tsv" ]
[ ! -e "$tmp/data/home-results.tsv" ]
[ ! -e "$tmp/data/stream-url.txt" ]
[ "$(cat "$tmp/data/quality.txt")" = 480 ]
[ "$(cat "$tmp/data/query.txt")" = 'science space' ]
[ -f "$tmp/data/radio-favourites.tsv" ]
[ ! -f "$tmp/data/radio-countries.tsv" ]
[ ! -f "$tmp/data/radio-stations.tsv" ]
grep -q 'theme=forest' "$tmp/data/settings.ini"
[ "$(find "$tmp/cache" -type f | wc -l)" -eq 0 ]
echo 'PASS: clear cache removes cached data and preserves preferences'
echo 'All MustardTube v0.9.9 backend tests passed.' 

# Prevent reintroduction of the regression: highlighting a result must not
# automatically initiate a heavyweight yt-dlp process.
if grep -q 'highlight_pending\|highlight_at' src/main.c; then
  echo 'FAIL: highlight-triggered extraction has returned' >&2; exit 1
fi
if ! grep -q 'show_video_details' src/main.c; then
  echo 'FAIL: expected details-page action missing' >&2; exit 1
fi
echo 'PASS: result highlighting does not start extraction'

# Node priority and verified fallback: mock the executables without network access.
printf '#!/bin/sh\nexit 0\n' > "$tmp/bin/node"
chmod +x "$tmp/bin/node"
rm -f "$tmp/data/cached-search-time.txt"
printf 'science space\n' > "$tmp/data/pending-search.txt"
backend search
grep -Fq -- "--js-runtimes node:$tmp/bin/node" "$MOCK_CALLS"
rm -f "$tmp/bin/node" "$tmp/data/cached-search-time.txt"
printf 'science space\n' > "$tmp/data/pending-search.txt"
backend search
grep -Fq -- "--js-runtimes quickjs:$tmp/bin/qjs" "$MOCK_CALLS"
echo 'PASS: Node preferred; QuickJS selected only when Node is absent'

# Live streams: two HLS URLs must be combined into one PRIVATE local A/V playlist.
# Normal single-URL VOD cache continues to work without re-extraction.
printf 'LIVE0000001\nActive YouTube live broadcast\n' > "$tmp/data/selected.txt"
printf '360\n' > "$tmp/data/quality.txt"
backend prefetch
[ "$(cat "$tmp/data/stream-kind.txt")" = live ]
[ "$(cat "$tmp/data/stream-url.txt")" = "$tmp/cache/live-hls.m3u8" ]
[ "$(stat -c '%a' "$tmp/cache/live-hls.m3u8")" = 600 ]
[ "$(cat "$tmp/data/status.txt")" = 'Live stream ready.' ]
grep -Fq '#EXT-X-MEDIA:TYPE=AUDIO' "$tmp/cache/live-hls.m3u8"
grep -Fq 'video360.m3u8?' "$tmp/cache/live-hls.m3u8"
grep -Fq 'audio234.m3u8?' "$tmp/cache/live-hls.m3u8"
grep -Fq 'RESOLUTION=640x360' "$tmp/cache/live-hls.m3u8"
grep -Fq 'bestvideo[protocol^=m3u8]' "$MOCK_CALLS"
[ "$(wc -l < "$tmp/cache/live-ready")" -eq 4 ]
live_calls=$(wc -l < "$MOCK_CALLS")
backend resolve
[ "$(wc -l < "$MOCK_CALLS")" -eq "$live_calls" ]
grep -q 'resolve cache=hit quality=360 stream=live' "$tmp/data/timing.log"
echo 480 > "$tmp/data/quality.txt"
backend prefetch
[ "$(wc -l < "$MOCK_CALLS")" -eq "$((live_calls+1))" ]
grep -Fq 'RESOLUTION=854x480' "$tmp/cache/live-hls.m3u8"
[ "$(cat "$tmp/data/stream-kind.txt")" = live ]
backend invalidate
[ ! -e "$tmp/cache/live-hls.m3u8" ]
[ ! -e "$tmp/cache/live-ready" ]
[ ! -e "$tmp/data/stream-kind.txt" ]
# Invalid selected IDs must never be interpreted as a shell or yt-dlp argument.
printf 'LIVE000000;\nInvalid\n' > "$tmp/data/selected.txt"
if backend prefetch; then exit 1; fi
# No URL or video identifier is written to timing diagnostics.
if grep -Fq 'cdn.example.invalid' "$tmp/data/timing.log"; then
  echo 'FAIL: HLS signed URL in diagnostic log' >&2; exit 1
fi
if ! grep -Fq -- '--live --title "$title" "$stream"' packaging/mux_launch.sh; then
  echo 'FAIL: Wasabi live handoff missing' >&2; exit 1
fi
echo 'PASS: live dual-HLS A/V, reuse, quality switch, private manifest and Wasabi live handoff'

# 720p live and recorded video must use real H.264 720p tracks.
printf 'LIVE0000001\nActive YouTube live broadcast\n' > "$tmp/data/selected.txt"
echo 720 > "$tmp/data/quality.txt"
backend prefetch
[ "$(cat "$tmp/data/stream-kind.txt")" = live ]
grep -Fq 'RESOLUTION=1280x720' "$tmp/cache/live-hls.m3u8"
grep -Fq 'video720.m3u8' "$tmp/cache/live-hls.m3u8"
grep -Fq '[height=720][vcodec^=avc1]' "$MOCK_CALLS"
[ "$(cat "$tmp/data/prefetch-ready.txt" | tail -n1)" = 720 ]
# Switching to 480 creates a new playlist rather than reusing the HD one.
echo 480 > "$tmp/data/quality.txt"
backend prefetch
grep -Fq 'RESOLUTION=854x480' "$tmp/cache/live-hls.m3u8"
# Unsupported HD streams must fail instead of claiming the lower-res variant is HD.
backend invalidate
echo 720 > "$tmp/data/quality.txt"
export MOCK_NO_HD=1
if backend prefetch; then echo 'FAIL: unsupported HD stream was accepted' >&2; exit 1; fi
grep -Fq 'HD 720p unavailable' "$tmp/data/status.txt"
unset MOCK_NO_HD
# Recorded video in HD mode now selects exact 720p H.264 video + HLS audio.
# The same 2-URL manifest is classified as vod_hls (not live), and the
# launcher intentionally uses VOD mode for Wasabi seeking support.
printf '00000000001\nRecorded test video\n' > "$tmp/data/selected.txt"
backend prefetch
[ "$(cat "$tmp/data/stream-kind.txt")" = vod_hls ]
[ "$(cat "$tmp/data/status.txt")" = 'Recorded HD 720p ready.' ]
grep -Fq 'RESOLUTION=1280x720' "$tmp/cache/live-hls.m3u8"
grep -Fq 'vod720.m3u8' "$tmp/cache/live-hls.m3u8"
grep -Fq 'vod-audio.m3u8' "$tmp/cache/live-hls.m3u8"
[ "$(sed -n '4p' "$tmp/cache/live-ready")" = vod_hls ]
calls=$(wc -l < "$MOCK_CALLS")
backend resolve
[ "$(wc -l < "$MOCK_CALLS")" -eq "$calls" ]
grep -q 'resolve cache=hit quality=720 stream=vod_hls' "$tmp/data/timing.log"
grep -Fq -- '--print-to-file %(live_status)s' "$MOCK_CALLS"
backend invalidate
[ ! -f "$tmp/cache/live-hls.m3u8" ]
# Old HD playback must not fall back to 480p while claiming 720p.
export MOCK_NO_HD=1
if backend prefetch; then echo 'FAIL: recorded HD silently fell back to 480p' >&2; exit 1; fi
unset MOCK_NO_HD
# Returning to 480p continues using the original single URL path.
echo 480 > "$tmp/data/quality.txt"
backend prefetch
[ "$(cat "$tmp/data/stream-kind.txt")" = vod ]
[ "$(cat "$tmp/data/status.txt")" = 'Ready to play.' ]
# No signed URLs written to logs. HLS manifests should be private.
if grep -R 'cdn.example.invalid' "$tmp/data"/*.log; then echo 'Signed stream URL leaked to logs' >&2; exit 1; fi
grep -Fq '"$WASABI" --title "$title" "$stream"' packaging/mux_launch.sh
if grep -Fq 'LIVE only; VOD uses 480p' src/main.c; then echo 'Old misleading HD label remains' >&2; exit 1; fi
echo 'PASS: genuine 720p recorded HLS, live/VOD separation, private playlist and VOD-mode launcher'

echo 1080 > "$tmp/data/quality.txt"
for sample in '00000000001 vod_hls' 'LIVE0000001 live'; do
    set -- $sample
    printf '%s\n' "$1" > "$tmp/data/selected.txt"
    backend invalidate
    backend prefetch
    [ "$(cat "$tmp/data/stream-kind.txt")" = "$2" ]
    grep -Fq 'RESOLUTION=1920x1080' "$tmp/cache/live-hls.m3u8"
    grep -Fq 'video1080.m3u8' "$tmp/cache/live-hls.m3u8"
done
backend invalidate
export MOCK_SINGLE_LIVE=1
backend prefetch
[ "$(cat "$tmp/data/stream-kind.txt")" = live_direct ]
calls=$(wc -l < "$MOCK_CALLS")
backend resolve
[ "$(wc -l < "$MOCK_CALLS")" -eq "$calls" ]
backend invalidate
export MOCK_NO_HD=1
if backend prefetch; then exit 1; fi
grep -Fq 'HD 1080p unavailable' "$tmp/data/status.txt"
echo 'PASS: exact 1080p recorded, live, direct-live reuse and unavailable formats'
