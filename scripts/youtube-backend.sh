#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# MustardTube v0.9.9. Node-first; experimental real H.264 HLS 720p for live and recorded videos.
set -eu
APP_DIR="${MUSTARDTUBE_APP_DIR:-/run/muos/storage/application/MustardTube}"
DATA="$APP_DIR/data"
umask 077
mkdir -p "$DATA/ytdlp-cache"
YTDLP="${MUSTARDTUBE_YTDLP:-$APP_DIR/bin/yt-dlp}"
QJS="${MUSTARDTUBE_QJS:-$APP_DIR/bin/qjs}"
NODE="${MUSTARDTUBE_NODE:-$APP_DIR/bin/node}"
JS_RUNTIME=''
# Node 24 resolves the same EJS challenge ~3x faster than bundled QuickJS
# on the target H700 (13s vs 41s in a controlled single-video test).
# Retain QuickJS only if Node is genuinely absent/nonexecutable.
choose_js_runtime() {
    if [ "${MUSTARDTUBE_FORCE_QJS:-0}" != 1 ] && [ -x "$NODE" ]; then
        JS_RUNTIME="node:$NODE"
    elif [ -x "$QJS" ]; then
        JS_RUNTIME="quickjs:$QJS"
    else
        status 'JavaScript runtime missing. Reinstall MustardTube.'
        return 1
    fi
}

YT_CACHE="$DATA/ytdlp-cache"
STATUS="$DATA/status.txt"
# Diagnostics contain timings only, never extractor output or signed URLs.
CACHE_DIR="${MUSTARDTUBE_CACHE_DIR:-/tmp/mustardtube-streams-$(id -u)}"
mkdir -p "$CACHE_DIR"
chmod 700 "$CACHE_DIR"
# Called only with the UI worker stopped or as its sole worker.
# Bound extractor metadata and diagnostics; these are soft limits per operation.
maintain_cache() {
    kb=$(du -sk "$YT_CACHE" 2>/dev/null | awk '{print $1}')
    case "$kb" in ''|*[!0-9]*) kb=0;; esac
    if [ "$kb" -gt 32768 ]; then rm -rf "$YT_CACHE"; mkdir -p "$YT_CACHE"; fi
    for log in "$DATA/timing.log" "$DATA/launch.log"; do
        if [ -f "$log" ] && [ "$(wc -c < "$log")" -gt 131072 ]; then
            tail -n 200 "$log" > "$log.tmp"; mv -f "$log.tmp" "$log"
        fi
    done
    rm -f "$CACHE_DIR"/resolve-*.tmp "$CACHE_DIR"/resolve-*.meta "$CACHE_DIR"/slot-*.tmp.* "$CACHE_DIR"/live-hls.tmp.* "$CACHE_DIR"/live-ready.tmp.*
    # Expired signed HLS manifests must not be retained indefinitely.
    rm -f "$CACHE_DIR"/direct-live.tmp.*
    if [ -f "$CACHE_DIR/direct-live" ]; then
        live_expiry=$(sed -n '3p' "$CACHE_DIR/direct-live")
        case "$live_expiry" in ''|*[!0-9]*) live_expiry=0;; esac
        [ "$live_expiry" -gt "$(date +%s)" ] || rm -f "$CACHE_DIR/direct-live"
    fi
    if [ -f "$CACHE_DIR/live-ready" ]; then
        old_live_until=$(sed -n '3p' "$CACHE_DIR/live-ready" 2>/dev/null || true)
        case "$old_live_until" in ''|*[!0-9]*) old_live_until=0;; esac
        if [ "$old_live_until" -le "$(date +%s)" ]; then
            rm -f "$CACHE_DIR/live-ready" "$CACHE_DIR/live-hls.m3u8"
        fi
    elif [ -f "$CACHE_DIR/live-hls.m3u8" ]; then
        rm -f "$CACHE_DIR/live-hls.m3u8"
    fi
    rm -f /tmp/mustardtube-thumb-???????????.jpg.tmp.*
    # Keep thumbnails bounded across reboots/old releases, not only on fetch.
    # Each decoded JPEG stays below 160 KB and only eight may be cached.
    for thumb in /tmp/mustardtube-thumb-???????????.jpg; do
        [ -f "$thumb" ] || continue
        thumb_bytes=$(wc -c < "$thumb" 2>/dev/null || echo 0)
        case "$thumb_bytes" in ''|*[!0-9]*) thumb_bytes=160001;; esac
        [ "$thumb_bytes" -le 160000 ] || rm -f "$thumb"
    done
    ls -t /tmp/mustardtube-thumb-???????????.jpg 2>/dev/null | tail -n +9 |
        while IFS= read -r old; do rm -f "$old"; done
    rm -f "$DATA"/prefetch-ready.txt.tmp.* "$DATA"/prefetch-failed.txt.tmp.* "$DATA"/stream-url.txt.tmp.* "$DATA"/stream-kind.txt.tmp.* "$DATA"/status.txt.tmp.* "$DATA"/home-results.tsv.tmp.* "$DATA"/home-updated.txt.tmp.* "$DATA"/home-status.txt.tmp.* "$DATA"/home-raw.tmp.*
}
maintain_cache
BATCH=8
MAX_RESULTS=120
MAX_SCAN=400
STREAM_TTL=300
LIVE_TTL=45 # only long enough for preview -> Wasabi handoff, never ordinary 5-minute caching
seconds() { date +%s; }
status() { printf '%s\n' "$1" > "$STATUS.tmp.$$"; mv -f "$STATUS.tmp.$$" "$STATUS"; }
count_lines() { awk 'END {print NR+0}' "$1"; }
prereq() {
    [ -x "$YTDLP" ] || { status 'YouTube extractor missing. Reinstall MustardTube.'; return 1; }
    choose_js_runtime || return 1
}
valid_id() { case "$1" in ''|*[!A-Za-z0-9_-]*) return 1;; esac; [ "${#1}" -eq 11 ]; }
read_selection() {
    id=$(sed -n '1p' "$DATA/selected.txt" 2>/dev/null || true)
    valid_id "$id" || { status 'Choose a video first.'; return 1; }
    quality=$(cat "$DATA/quality.txt" 2>/dev/null || true)
    case "$quality" in 480|720|1080) ;; *) quality=360;; esac
}
# 60s backend watchdog: hardware testing resolved a stream in 41s at 1.8 GHz.
# The UI waits longer (66s) so it never cancels a still-valid backend attempt.
# A watchdog bounds the whole extraction, including its internal retries.
# The UI owns the process group, so navigation cancels this and its children.
run_resolver() {
    # Keep the proven 360p/480p extraction arguments unchanged.
    # Only HD needs an extra (local-only) live_status print to distinguish
    # recorded two-track HLS from a live two-track HLS broadcast.
    if [ "$quality" -ge 720 ]; then
        set -- --print-to-file '%(live_status)s' "$meta"
    else
        set --
    fi
    "$YTDLP" --ignore-config --no-warnings --no-progress \
      --socket-timeout 10 --extractor-retries 1 --retries 1 \
      --cache-dir "$YT_CACHE" --js-runtimes "$JS_RUNTIME" \
      --no-playlist --get-url "$@" -f "$fmt" \
      -- "https://www.youtube.com/watch?v=$id" > "$tmp" 2>/dev/null &
    extractor=$!
    deadline=$(( $(seconds) + ${MUSTARDTUBE_RESOLVE_TIMEOUT:-60} ))
    while kill -0 "$extractor" 2>/dev/null; do
        if [ "$(seconds)" -ge "$deadline" ]; then
            kill -KILL "$extractor" 2>/dev/null || true
            wait "$extractor" 2>/dev/null || true
            return 4
        fi
        sleep 0.2
    done
    rc=0
    wait "$extractor" || rc=$?
    return "$rc"
}
# HLS manifests (live and VOD) are never stored in an ordinary stream-cache slot.
# They live in a private ephemeral directory for at most LIVE_TTL seconds.
# Back-to-back UI prefetch and launcher resolve reuse one manifest; playback
# removes it immediately on return from Wasabi.
playable_hls_cache() {
    [ -s "$CACHE_DIR/live-ready" ] && [ -s "$CACHE_DIR/live-hls.m3u8" ] || return 1
    live_id=$(sed -n '1p' "$CACHE_DIR/live-ready")
    live_quality=$(sed -n '2p' "$CACHE_DIR/live-ready")
    live_until=$(sed -n '3p' "$CACHE_DIR/live-ready")
    case "$live_until" in ''|*[!0-9]*) return 1;; esac
    hls_kind=$(sed -n '4p' "$CACHE_DIR/live-ready")
    case "$hls_kind" in live|vod_hls) ;; *) return 1;; esac
    [ "$live_id" = "$id" ] && [ "$live_quality" = "$quality" ] && [ "$live_until" -gt "$(seconds)" ]
}
# yt-dlp --get-url outputs two lines for an HLS video+audio selection.
# Never pass these as two separate Wasabi URLs: generate the HLS audio group
# plus video variant, exactly as confirmed working in the device Live Probe.
create_hls_manifest() {
    [ "$(count_lines "$tmp")" -eq 2 ] || return 1
    video_url=$(sed -n '1p' "$tmp")
    audio_url=$(sed -n '2p' "$tmp")
    case "$video_url" in https://*) ;; *) return 1;; esac
    case "$audio_url" in https://*) ;; *) return 1;; esac
    [ "${#video_url}" -le 12000 ] && [ "${#audio_url}" -le 12000 ] || return 1
    # Quotes or controls would corrupt the M3U8 quoted audio URI attribute.
    if printf '%s\n%s\n' "$video_url" "$audio_url" | LC_ALL=C grep -Eq '["[:cntrl:]]'; then return 1; fi
    # Reject expired source URLs, if they advertise an expiry timestamp.
    live_until=$(( $(seconds) + LIVE_TTL ))
    for url in "$video_url" "$audio_url"; do
        expiry=$(printf '%s' "$url" | sed -n 's/.*[?&]expire=\([0-9][0-9]*\).*/\1/p')
        case "$expiry" in ''|*[!0-9]*) ;; *)
            [ "$expiry" -ge "$live_until" ] || live_until=$((expiry - 15));;
        esac
    done
    [ "$live_until" -gt "$(( $(seconds) + 5 ))" ] || return 1
    if [ "$quality" = 360 ]; then
        resolution=640x360
        codecs='avc1.4D401E,mp4a.40.2'
        bandwidth=1200000
    elif [ "$quality" = 480 ]; then
        resolution=854x480
        codecs='avc1.4D401F,mp4a.40.2'
        bandwidth=1700000
    else
        # Only a matching H.264 720p HLS rendition is accepted in HD mode.
        # Do not mislabel a 360p/480p fallback as a 720p stream.
        resolution=1280x720
        codecs='avc1.4D401F,mp4a.40.2'
        bandwidth=3300000
        if [ "$quality" = 1080 ]; then
            resolution=1920x1080
            codecs=avc1.640028,mp4a.40.2
            bandwidth=6500000
        fi
    fi
    manifest="$CACHE_DIR/live-hls.tmp.$$"
    {
        printf '%s\n' '#EXTM3U' '#EXT-X-VERSION:3'
        printf '#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="audio",NAME="Main",DEFAULT=YES,AUTOSELECT=YES,URI="%s"\n' "$audio_url"
        printf '#EXT-X-STREAM-INF:BANDWIDTH=%s,RESOLUTION=%s,CODECS="%s",AUDIO="audio"\n' "$bandwidth" "$resolution" "$codecs"
        printf '%s\n' "$video_url"
    } > "$manifest" || return 1
    chmod 600 "$manifest"
    mv -f "$manifest" "$CACHE_DIR/live-hls.m3u8"
    printf '%s\n%s\n%s\n%s\n' "$id" "$quality" "$live_until" "$hls_kind" > "$CACHE_DIR/live-ready.tmp.$$"
    mv -f "$CACHE_DIR/live-ready.tmp.$$" "$CACHE_DIR/live-ready"
    uri="$CACHE_DIR/live-hls.m3u8"
    until="$live_until"
    stream_kind="$hls_kind"
    return 0
}
resolve_core() {
    read_selection || return 3
    slot=$(printf '%s:%s' "$id" "$quality" | cksum | awk '{print $1 % 4}')
    cache="$CACHE_DIR/slot-$slot"
    now=$(seconds)
    if [ -f "$CACHE_DIR/direct-live" ]; then
        {
            read -r saved_id && read -r saved_quality && read -r until && read -r uri
        } < "$CACHE_DIR/direct-live" || until=0
        if [ "$id" = "$saved_id" ] && [ "$quality" = "$saved_quality" ] && [ "$until" -gt "$now" ]; then
            stream_kind=live_direct
            publish_stream
            return 0
        fi
        rm -f "$CACHE_DIR/direct-live"
    fi
    if playable_hls_cache; then
        uri="$CACHE_DIR/live-hls.m3u8"
        until="$live_until"
        stream_kind="$hls_kind"
        publish_stream
        printf 'resolve cache=hit quality=%s stream=%s\n' "$quality" "$stream_kind" >> "$DATA/timing.log"
        return 0
    fi
    # Remove stale/other-video live URLs before starting a new request.
    rm -f "$CACHE_DIR/live-ready" "$CACHE_DIR/live-hls.m3u8"
    if [ -f "$cache" ]; then
        saved_id=$(sed -n '1p' "$cache")
        saved_quality=$(sed -n '2p' "$cache")
        until=$(sed -n '3p' "$cache")
        case "$until" in ''|*[!0-9]*) until=0;; esac
        if [ "$id" = "$saved_id" ] && [ "$quality" = "$saved_quality" ] && [ "$now" -lt "$until" ]; then
            uri=$(sed -n '4p' "$cache")
            case "$uri" in https://*|http://*)
                stream_kind=vod
                publish_stream
                printf 'resolve cache=hit quality=%s stream=vod\n' "$quality" >> "$DATA/timing.log"
                return 0;;
            esac
        fi
    fi
    tmp="$CACHE_DIR/resolve-$$.tmp"
    meta="$CACHE_DIR/resolve-$$.meta"
    rm -f "$meta"
    # Normal videos retain the v0.9.6 combined audio/video format preference.
    # Only when none exists do we select a separate *HLS* video+audio pair.
    # yt-dlp resolves both URLs in the SAME extractor invocation.
    if [ "$quality" = 360 ]; then
        normal_fmt='best[ext=mp4][height<=360][vcodec^=avc1][acodec!=none]/best[ext=mp4][height<=360][acodec!=none]/best[height<=360][acodec!=none]/best[ext=mp4][height<=480][acodec!=none]/best[height<=480][acodec!=none]/best[ext=mp4][acodec!=none]/best[acodec!=none]'
        live_fmt='bestvideo[protocol^=m3u8][height<=360][vcodec^=avc1]+bestaudio[protocol^=m3u8]/bestvideo[protocol^=m3u8][height<=480]+bestaudio[protocol^=m3u8]'
    elif [ "$quality" = 480 ]; then
        normal_fmt='best[ext=mp4][height<=480][vcodec^=avc1][acodec!=none]/best[ext=mp4][height<=480][acodec!=none]/best[height<=480][acodec!=none]/best[ext=mp4][acodec!=none]/best[acodec!=none]'
        live_fmt='bestvideo[protocol^=m3u8][height<=480][vcodec^=avc1]+bestaudio[protocol^=m3u8]/bestvideo[protocol^=m3u8][height<=480]+bestaudio[protocol^=m3u8]'
    else
        # Genuine 720p H.264 for BOTH recorded and live videos. Prefer an
        # actual combined 720p source when offered; otherwise select exactly
        # one 720p HLS video track and one HLS audio track in a single request.
        # No silent 480p substitution in the HD mode.
        hd_muxed="best[height=$quality][vcodec^=avc1][acodec!=none]"
        hd_hls="bestvideo[protocol^=m3u8][height=$quality][vcodec^=avc1]+bestaudio[protocol^=m3u8]"
        normal_fmt="$hd_muxed"
        live_fmt="$hd_hls"
    fi
    fmt="$normal_fmt/$live_fmt"
    start_time=$(seconds)
    status 'Preparing video...'
    case "$JS_RUNTIME" in node:*) solver=node;; *) solver=quickjs;; esac
    if run_resolver; then
        lines=$(count_lines "$tmp")
        if [ "$lines" -eq 1 ]; then
            uri=$(sed -n '1p' "$tmp")
            case "$uri" in https://*|http://*)
                if [ "$quality" -ge 720 ] && [ "$(cat "$meta" 2>/dev/null)" = is_live ]; then
                    stream_kind=live_direct
                    until=$(( $(seconds) + LIVE_TTL ))
                    printf '%s\n' "$id" "$quality" "$until" "$uri" > "$CACHE_DIR/direct-live.tmp.$$"
                    mv -f "$CACHE_DIR/direct-live.tmp.$$" "$CACHE_DIR/direct-live"
                    publish_stream
                    rm -f "$tmp" "$meta"
                    return 0
                fi
                until=$(( $(seconds) + STREAM_TTL ))
                expiry=$(printf '%s' "$uri" | sed -n 's/.*[?&]expire=\([0-9][0-9]*\).*/\1/p')
                case "$expiry" in ''|*[!0-9]*) ;; *)
                    [ "$expiry" -ge "$until" ] || until=$((expiry - 15));;
                esac
                if [ "$until" -gt "$(seconds)" ]; then
                    printf '%s\n%s\n%s\n%s\n' "$id" "$quality" "$until" "$uri" > "$cache.tmp.$$"
                    mv -f "$cache.tmp.$$" "$cache"
                    stream_kind=vod
                    publish_stream
                    rm -f "$tmp" "$meta"
                    printf 'resolve cache=miss duration=%ss quality=%s solver=%s stream=vod\n' "$(( $(seconds)-start_time ))" "$quality" "$solver" >> "$DATA/timing.log"
                    return 0
                fi;;
            esac
        elif [ "$lines" -eq 2 ]; then
            if [ "$quality" -lt 720 ]; then
                # Existing 360p/480p fallback path was live-only in v0.9.8.
                hls_kind=live
            else
                live_status=$(sed -n '1p' "$meta" 2>/dev/null || true)
                case "$live_status" in
                    is_live) hls_kind=live;;
                    not_live|was_live|post_live) hls_kind=vod_hls;;
                    *) hls_kind=unknown;;
                esac
            fi
            if [ "$hls_kind" != unknown ] && create_hls_manifest; then
                publish_stream
                rm -f "$tmp" "$meta"
                printf 'resolve cache=miss duration=%ss quality=%s solver=%s stream=%s\n' "$(( $(seconds)-start_time ))" "$quality" "$solver" "$hls_kind" >> "$DATA/timing.log"
                return 0
            fi
        fi
    fi
    rm -f "$tmp" "$meta" "$CACHE_DIR/live-hls.tmp.$$" "$CACHE_DIR/live-ready.tmp.$$"
    printf 'resolve outcome=failed duration=%ss quality=%s solver=%s\n' "$(( $(seconds)-start_time ))" "$quality" "$solver" >> "$DATA/timing.log"
    if [ "$quality" -ge 720 ]; then
        status "HD ${quality}p unavailable for this video. Press Y to switch quality."
    else
        status 'Video or live stream unavailable. Press A to retry.'
    fi
    return 4
}
publish_stream() {
    printf '%s\n' "$uri" > "$DATA/stream-url.txt.tmp.$$"
    mv -f "$DATA/stream-url.txt.tmp.$$" "$DATA/stream-url.txt"
    printf '%s\n' "$stream_kind" > "$DATA/stream-kind.txt.tmp.$$"
    mv -f "$DATA/stream-kind.txt.tmp.$$" "$DATA/stream-kind.txt"
    printf '%s\n%s\n%s\n' "$id" "$until" "$quality" > "$DATA/prefetch-ready.txt.tmp.$$"
    mv -f "$DATA/prefetch-ready.txt.tmp.$$" "$DATA/prefetch-ready.txt"
    if [ "$stream_kind" = live ] || [ "$stream_kind" = live_direct ]; then status 'Live stream ready.'; elif [ "$stream_kind" = vod_hls ]; then status "Recorded HD ${quality}p ready."; else status 'Ready to play.'; fi
}
# Keep diagnostics coarse: never persist queries, full stderr, URLs or tokens.
search_error_category() {
    if grep -Eiq '429|too many requests|rate.limit' "$err"; then echo rate_limited
    elif grep -Eiq '403|forbidden|sign in|bot' "$err"; then echo access_denied
    elif grep -Eiq 'timed out|timeout|connection|network|dns|unreachable' "$err"; then echo network
    elif grep -Eiq 'certificate|tls|ssl' "$err"; then echo tls
    elif grep -Eiq 'quickjs|javascript|challenge|js runtime|js-runtimes' "$err"; then echo javascript
    elif grep -Eiq 'unsupported|unknown option|no such file|permission denied' "$err"; then echo runtime
    else echo extractor; fi
}
search_batch() {
    # Same working ytsearch request as v0.9; errors now carry safe categories.
    err="$DATA/search-stderr-$$.tmp"
    if "$YTDLP" --ignore-config --no-warnings --no-progress \
      --socket-timeout 12 --extractor-retries 1 --retries 1 \
      --cache-dir "$YT_CACHE" --js-runtimes "$JS_RUNTIME" \
      --flat-playlist --playlist-end "$end" \
      --print '%(id)s|||%(title)s|||%(channel)s|||%(duration_string)s' \
      -- "ytsearch${end}:$query" > "$raw" 2>"$err"; then
        rm -f "$err"
        return 0
    fi
    reason=$(search_error_category)
    rm -f "$err"
    printf 'search_failure category=%s mode=%s scanned=%s\n' "$reason" "$mode" "$end" >> "$DATA/timing.log"
    case "$reason" in
      network) status 'Search network timeout. Check Wi-Fi and retry.';;
      rate_limited) status 'YouTube is limiting requests. Try again later.';;
      access_denied) status 'YouTube blocked this request. Try later.';;
      tls) status 'Secure connection failed. Check device time.';;
      javascript) status 'YouTube extraction needs an updated runtime.';;
      runtime) status 'YouTube runtime error. Check install.';;
      *) status 'YouTube search failed. See diagnostics.';;
    esac
    return 1
}
merge_results() {
    awk -v cap="$MAX_RESULTS" '{m=index($0,"|||"); if(!m)next; id=substr($0,1,m-1);
      if(length(id)!=11 || id ~ /[^A-Za-z0-9_-]/ || seen[id]++)next;
      if(++n<=cap)print}' "$@" > "$merged"
}
fetch_thumb() {
    [ "${MUSTARDTUBE_SKIP_THUMBNAILS:-0}" = 1 ] && return 0
    vid="$1"
    file="/tmp/mustardtube-thumb-$vid.jpg"
    # Ignore corrupt/oversized leftovers rather than pinning a bad cache entry.
    if [ -s "$file" ]; then
        old_bytes=$(wc -c < "$file" 2>/dev/null || echo 160001)
        case "$old_bytes" in ''|*[!0-9]*) old_bytes=160001;; esac
        [ "$old_bytes" -le 160000 ] && return 0
        rm -f "$file"
    fi
    tmp="$file.tmp.$$"
    url="https://i.ytimg.com/vi/$vid/mqdefault.jpg"
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL --connect-timeout 2 --max-time 6 --max-filesize 160000 "$url" -o "$tmp" >/dev/null 2>&1 || { rm -f "$tmp";return 0; }
    elif command -v wget >/dev/null 2>&1; then
        wget -q -T 6 -O "$tmp" "$url" >/dev/null 2>&1 || { rm -f "$tmp";return 0; }
    else
        return 0
    fi
    [ -s "$tmp" ] || { rm -f "$tmp";return 0; }
    size=$(wc -c < "$tmp")
    if [ "$size" -gt 160000 ]; then rm -f "$tmp";return 0;fi
    mv -f "$tmp" "$file"
    # These names are generated from validated IDs, never user-supplied paths.
    ls -t /tmp/mustardtube-thumb-???????????.jpg 2>/dev/null | tail -n +9 |
        while IFS= read -r old; do rm -f "$old"; done
}


# Report only disposable MustardTube data. Deliberately exclude any
# future downloads directory, preferences, and user-owned media on TF2.
cache_size_kib() {
    sum=0
    for path in "$YT_CACHE" "$CACHE_DIR" "$DATA/radio-logo-cache"; do
        [ -d "$path" ] || continue
        kb=$(du -sk "$path" 2>/dev/null | awk '{print $1}')
        case "$kb" in ''|*[!0-9]*) kb=0;; esac
        sum=$((sum + kb))
    done
    for path in "$DATA"/home-*-results.tsv "$DATA"/home-*-updated.txt "$DATA"/home-*-status.txt \
                "$DATA"/home-*-pool.tsv "$DATA"/home-*-position.txt "$DATA"/home-*-scan.txt "$DATA"/home-*-query.txt \
                "$DATA/home-results.tsv" "$DATA/home-updated.txt" \
                "$DATA/search-results.tsv" "$DATA/cached-query.txt" \
                "$DATA/cached-search-time.txt" "$DATA/search-raw.tmp" \
                "$DATA/search-merged.tmp" "$DATA/home-status.txt"; do
        [ -f "$path" ] || continue
        bytes=$(wc -c < "$path" 2>/dev/null || echo 0)
        case "$bytes" in ''|*[!0-9]*) bytes=0;; esac
        sum=$((sum + (bytes + 1023) / 1024))
    done
    for path in /tmp/mustardtube-thumb-???????????.jpg; do
        [ -f "$path" ] || continue
        bytes=$(wc -c < "$path" 2>/dev/null || echo 0)
        case "$bytes" in ''|*[!0-9]*) bytes=0;; esac
        sum=$((sum + (bytes + 1023) / 1024))
    done
    printf '%s\n' "$sum"
}

case "${1:-}" in
home|home-thumbs)
    # Six independent, cached discovery feeds. Only the selected category loads.
    mode="$1"
    category="${2:-explore}"
    case "$category" in explore|popular|foryou|music|gaming|tech) ;; *) exit 2;; esac
    out="$DATA/home-$category-results.tsv"
    stamp="$DATA/home-$category-updated.txt"
    note="$DATA/home-$category-status.txt"
    home_status() { printf '%s\n' "$1" > "$note.tmp.$$"; mv -f "$note.tmp.$$" "$note"; }
    if [ "$mode" = home ]; then
        prereq || exit 2
        home_status 'Loading real YouTube videos...'
        raw="$DATA/home-raw.tmp.$$"
        prepared="$out.tmp.$$"
        trap 'rm -f "$raw" "$prepared" "$prepared.pool" "$prepared.batch" "$prepared.merge"' EXIT HUP INT TERM
        started=$(seconds)
        # Search-based discovery; not official chart rankings or YouTube recommendations.
        case "$category" in
          explore) discovery_query="${MUSTARDTUBE_HOME_QUERY:-popular videos today}";;
          popular) discovery_query='most watched popular videos this week';;
          music) discovery_query='popular music official music videos';;
          gaming) discovery_query='popular gaming video creators';;
          tech) discovery_query='technology reviews tutorials';;
          foryou)
            # A small local interest signal: last played video title, not a YouTube account.
            # No history means no fabricated personal recommendations.
            recent_title=$(awk -F '[|][|][|]' 'NF>=2 {print substr($2,1,70);exit}' "$DATA/watch-history.tsv" 2>/dev/null || true)
            if [ -z "$recent_title" ]; then
                home_status 'For You: watch a video to build local recommendations.'
                exit 0
            fi
            discovery_query="videos similar to $recent_title";;
        esac
    fi
    if [ "$mode" = home ]; then
    # Four cards remain visible. Successive Refresh presses cycle through a
    # per-category discovery pool. Fetch 12 more IDs only when needed; do NOT
    # query 100+ items or download 100 thumbnails on every Home refresh.
    pool="$DATA/home-$category-pool.tsv"
    pointer="$DATA/home-$category-position.txt"
    scanfile="$DATA/home-$category-scan.txt"
    queryfile="$DATA/home-$category-query.txt"
    max_home_scan=600 # Safety bound well beyond 100; independent of visible cards.
    saved_query=$(cat "$queryfile" 2>/dev/null || true)
    position=$(cat "$pointer" 2>/dev/null || true)
    scanned=$(cat "$scanfile" 2>/dev/null || true)
    case "$position" in ''|*[!0-9]*) position=0;; esac
    case "$scanned" in ''|*[!0-9]*) scanned=0;; esac
    if [ "$saved_query" != "$discovery_query" ]; then
        position=0;scanned=0
        : > "$prepared.pool"
    elif [ -f "$pool" ]; then
        cp "$pool" "$prepared.pool"
    else
        : > "$prepared.pool"
        position=0;scanned=0
    fi
    # One new search page per refresh, unless it contains only duplicate IDs.
    # Bound retries to avoid long waits on a slow network.
    tries=0
    total=$(count_lines "$prepared.pool")
    while [ "$position" -ge "$total" ] && [ "$scanned" -lt "$max_home_scan" ] && [ "$tries" -lt 3 ]; do
        tries=$((tries+1))
        from=$((scanned+1));to=$((scanned+12))
        [ "$to" -le "$max_home_scan" ] || to=$max_home_scan
        # yt-dlp skips older playlist entries instead of printing all past cards.
        if command -v timeout >/dev/null 2>&1; then
            timeout 50 "$YTDLP" --ignore-config --no-progress --no-warnings \
              --socket-timeout 12 --extractor-retries 1 --retries 1 \
              --cache-dir "$YT_CACHE" --js-runtimes "$JS_RUNTIME" \
              --flat-playlist --playlist-start "$from" --playlist-end "$to" \
              --print '%(id)s|||%(title)s|||%(channel)s|||%(duration_string)s' \
              -- "ytsearch${to}:$discovery_query" > "$raw" 2>/dev/null || { home_status 'Unable to refresh; previous cards kept.'; exit 4; }
        else
            "$YTDLP" --ignore-config --no-progress --no-warnings \
              --socket-timeout 12 --extractor-retries 1 --retries 1 \
              --cache-dir "$YT_CACHE" --js-runtimes "$JS_RUNTIME" \
              --flat-playlist --playlist-start "$from" --playlist-end "$to" \
              --print '%(id)s|||%(title)s|||%(channel)s|||%(duration_string)s' \
              -- "ytsearch${to}:$discovery_query" > "$raw" 2>/dev/null || { home_status 'Unable to refresh; previous cards kept.'; exit 4; }
        fi
        raw_count=$(count_lines "$raw")
        awk -F '[|][|][|]' '
          NF >= 4 && length($1)==11 && $1 ~ /^[A-Za-z0-9_-]+$/ && $2!="" && $2!="NA" && !seen[$1]++ {
            gsub(/[\r\n\t]/," ",$2);gsub(/[\r\n\t]/," ",$3);gsub(/[\r\n\t]/," ",$4);
            title=substr($2,1,175);channel=substr($3,1,83);dur=substr($4,1,23);
            if(channel=="" || channel=="NA")channel="YouTube";
            if(dur=="NA")dur="";
            printf "%s|||%s|||%s|||%s\n",$1,title,channel,dur;
          }' "$raw" > "$prepared.batch"
        awk -F '[|][|][|]' '!seen[$1]++' "$prepared.pool" "$prepared.batch" > "$prepared.merge"
        mv -f "$prepared.merge" "$prepared.pool"
        scanned=$to
        total=$(count_lines "$prepared.pool")
        # Fewer than requested entries indicates the search has ended.
        if [ "$raw_count" -lt "$((to-from+1))" ]; then scanned=$max_home_scan;break;fi
    done
    # No more unique videos: preserve the last four rather than looping or
    # showing fake duplicates.  A new search/category starts a new pool.
    if [ "$position" -ge "$total" ]; then
        home_status 'No additional discovery videos. Try another category.'
        exit 0
    fi
    awk -v from="$((position+1))" -v to="$((position+4))" \
        'NR>=from && NR<=to {print}' "$prepared.pool" > "$prepared"
    count=$(count_lines "$prepared")
    [ "$count" -gt 0 ] || { home_status 'No discovery videos available; press X to retry.'; exit 4; }
    mv -f "$prepared.pool" "$pool"
    printf '%s\n' "$discovery_query" > "$queryfile.tmp.$$";mv -f "$queryfile.tmp.$$" "$queryfile"
    printf '%s\n' "$((position+count))" > "$pointer.tmp.$$";mv -f "$pointer.tmp.$$" "$pointer"
    printf '%s\n' "$scanned" > "$scanfile.tmp.$$";mv -f "$scanfile.tmp.$$" "$scanfile"
    mv -f "$prepared" "$out"
    seconds > "$stamp.tmp.$$";mv -f "$stamp.tmp.$$" "$stamp"
    home_status "$count videos loaded. Press X for another four."
    printf 'home mode=%s duration=%ss count=%s pool=%s scanned=%s\n' "$category" "$(( $(seconds)-started ))" "$count" "$total" "$scanned" >> "$DATA/timing.log"
    if [ "$category" = explore ]; then
        cp "$out" "$DATA/home-results.tsv"
        cp "$stamp" "$DATA/home-updated.txt"
        cp "$note" "$DATA/home-status.txt"
    fi
    fi # end fresh lookup; cache-only mode fetches thumbnails without yt-dlp.
    # At most four visible JPEG thumbnails; no media payload download.
    [ -f "$out" ] || exit 0
    sed -n 's/|||.*//p' "$out" | while IFS= read -r thumb_id; do
        valid_id "$thumb_id" || continue
        fetch_thumb "$thumb_id"
    done
    ;;
thumbs)
    # Search screen requests only the FOUR currently visible thumbnails.
    first="${2:-0}"
    case "$first" in ''|*[!0-9]*) exit 2;; esac
    [ "$first" -le 116 ] || exit 2
    [ -f "$DATA/search-results.tsv" ] || exit 0
    awk -F '[|][|][|]' -v from="$((first + 1))" -v to="$((first + 4))"       'NR>=from && NR<=to && length($1)==11 && $1~/^[A-Za-z0-9_-]+$/ {print $1}' "$DATA/search-results.tsv" |        while IFS= read -r thumb_id; do valid_id "$thumb_id" && fetch_thumb "$thumb_id"; done
    ;;
remember)
    # Store a maximum of 12 recently PLAYED titles only. No credentials or URLs.
    video_id=$(sed -n '1p' "$DATA/selected.txt" 2>/dev/null || true)
    valid_id "$video_id" || exit 0
    video_title=$(sed -n '2p' "$DATA/selected.txt" 2>/dev/null | tr '\r\n\t|' '    ' | cut -c 1-100 || true)
    [ -n "$video_title" ] || exit 0
    { printf '%s|||%s\n' "$video_id" "$video_title"
      if [ -f "$DATA/watch-history.tsv" ]; then
        awk -F '[|][|][|]' -v v="$video_id" 'length($1)==11 && $1!=v && $2!="" && ++n<=11 {print}' "$DATA/watch-history.tsv"
      fi
    } > "$DATA/watch-history.tsv.tmp.$$"
    mv -f "$DATA/watch-history.tsv.tmp.$$" "$DATA/watch-history.tsv"
    # Don't show a previous video's recommendations under For You.
    rm -f "$DATA/home-foryou-updated.txt" "$DATA/home-foryou-results.tsv" \
      "$DATA/home-foryou-pool.tsv" "$DATA/home-foryou-position.txt" "$DATA/home-foryou-scan.txt" "$DATA/home-foryou-query.txt"
    ;;
clear-history)
    rm -f "$DATA/watch-history.tsv" "$DATA/home-foryou-results.tsv" "$DATA/home-foryou-updated.txt" "$DATA/home-foryou-status.txt" \
      "$DATA/home-foryou-pool.tsv" "$DATA/home-foryou-position.txt" "$DATA/home-foryou-scan.txt" "$DATA/home-foryou-query.txt"
    status 'Local viewing history cleared; other preferences kept.'
    ;;
cache-size)
    printf 'Disposable MustardTube cache: %s KiB (excludes offline videos and preferences)\n' "$(cache_size_kib)"
    ;;
clear-cache)
    # Keep query, quality, theme and any future user-saved downloads.
    before_kib=$(cache_size_kib)
    rm -rf "$YT_CACHE"
    mkdir -p "$YT_CACHE"
    rm -f "$CACHE_DIR/direct-live" "$CACHE_DIR"/slot-* "$CACHE_DIR"/resolve-*.tmp "$CACHE_DIR"/live-hls.m3u8 "$CACHE_DIR"/live-ready
    rm -f /tmp/mustardtube-thumb-???????????.jpg /tmp/mustardtube-thumb-???????????.jpg.tmp.*
    # Radio station directories are disposable; favourites remain user data.
    rm -f "$DATA/radio-countries.tsv" "$DATA/radio-stations.tsv" "$DATA/radio-status.txt" "$DATA/radio-pagination.tsv"
    # Radio logos are disposable, favourites and country choice are retained.
    rm -rf "$DATA/radio-logo-cache"
    rm -f "$DATA"/home-*-results.tsv "$DATA"/home-*-updated.txt "$DATA"/home-*-status.txt \
      "$DATA"/home-*-pool.tsv "$DATA"/home-*-position.txt "$DATA"/home-*-scan.txt "$DATA"/home-*-query.txt \
      "$DATA/home-results.tsv" "$DATA/home-updated.txt" "$DATA/home-status.txt" "$DATA/selected-origin.txt" \
      "$DATA/search-results.tsv" "$DATA/cached-query.txt" "$DATA/cached-search-time.txt" \
      "$DATA/search-cursor.txt" "$DATA/more-available.txt" "$DATA/next-result-index.txt" \
      "$DATA/prefetch-ready.txt" "$DATA/prefetch-failed.txt" "$DATA/stream-url.txt" "$DATA/stream-kind.txt" \
      "$DATA/youtube.log" "$DATA/timing.log" "$DATA/search-raw.tmp" \
      "$DATA/search-merged.tmp" "$DATA/home-raw.tmp" \
      "$DATA/search-stderr-"*.tmp
    # Retire v0.8's single-entry cache too when upgrading.
    rm -f "/tmp/mustardtube-recent-stream-$(id -u).txt"
    after_kib=$(cache_size_kib)
    reclaimed=$((before_kib - after_kib))
    [ "$reclaimed" -ge 0 ] || reclaimed=0
    status "Cache cleared (~${reclaimed} KiB). Settings and saved videos kept."
    ;;
search|more)
    prereq || exit 2
    mode="$1"
    old_count=0
    if [ "$mode" = search ]; then
        query=$(cat "$DATA/pending-search.txt" 2>/dev/null || true)
        rm -f "$DATA/pending-search.txt"
        [ -n "$query" ] || { status 'Enter a search phrase.'; exit 3; }
        saved_query=$(cat "$DATA/cached-query.txt" 2>/dev/null || true)
        saved_time=$(cat "$DATA/cached-search-time.txt" 2>/dev/null || true)
        case "$saved_time" in ''|*[!0-9]*) saved_time=0;; esac
        age=$(( $(seconds)-saved_time ))
        if [ "$query" = "$saved_query" ] && [ "$age" -ge 0 ] && [ "$age" -lt 600 ] && [ -s "$DATA/search-results.tsv" ]; then
            printf '0\n' > "$DATA/next-result-index.txt"
            status "$(count_lines "$DATA/search-results.tsv") videos loaded."
            exit 0
        fi
        end=$BATCH
    else
        query=$(cat "$DATA/cached-query.txt" 2>/dev/null || true)
        [ -n "$query" ] && [ -s "$DATA/search-results.tsv" ] || { status 'Search for a video first.'; exit 3; }
        old_count=$(count_lines "$DATA/search-results.tsv")
        cursor=$(cat "$DATA/search-cursor.txt" 2>/dev/null || true)
        case "$cursor" in ''|*[!0-9]*) cursor=$old_count;; esac
        if [ "$old_count" -ge "$MAX_RESULTS" ] || [ "$cursor" -ge "$MAX_SCAN" ]; then
            echo 0 > "$DATA/more-available.txt"
            status 'Search limit reached. Try a more specific search.'; exit 0
        fi
        end=$((cursor+BATCH))
        [ "$end" -le "$MAX_SCAN" ] || end=$MAX_SCAN
    fi
    raw="$DATA/search-raw.tmp"
    merged="$DATA/search-merged.tmp"
    status 'Searching YouTube...'
    started=$(seconds)
    if search_batch; then
        raw_count=$(count_lines "$raw")
        if [ "$mode" = more ]; then merge_results "$DATA/search-results.tsv" "$raw";
        else merge_results "$raw"; fi
        count=$(count_lines "$merged")
        mv -f "$merged" "$DATA/search-results.tsv"
        printf '%s\n' "$query" > "$DATA/cached-query.txt"
        seconds > "$DATA/cached-search-time.txt"
        printf '%s\n' "$end" > "$DATA/search-cursor.txt"
        if [ "$count" -lt "$MAX_RESULTS" ] && [ "$end" -lt "$MAX_SCAN" ] && [ "$raw_count" -ge "$end" ]; then
            echo 1 > "$DATA/more-available.txt"
            if [ "$mode" = more ] && [ "$count" -eq "$old_count" ]; then status 'No new videos in this batch. Press X to keep looking.';
            else status "$count videos loaded. Press X for more."; fi
        else
            echo 0 > "$DATA/more-available.txt"
            status "$count videos loaded. End of results."
        fi
        if [ "$count" -gt "$old_count" ]; then printf '%s\n' "$old_count" > "$DATA/next-result-index.txt";
        else printf '0\n' > "$DATA/next-result-index.txt"; fi
        printf 'search mode=%s duration=%ss results=%s scanned=%s\n' "$mode" "$(( $(seconds)-started ))" "$count" "$end" >> "$DATA/timing.log"
    else
        if [ "$mode" = search ]; then rm -f "$DATA/search-results.tsv" "$DATA/more-available.txt" "$DATA/search-cursor.txt"; fi
        # search_batch sets a specific, non-sensitive error. Keep it visible.
        rm -f "$raw" "$merged"
        exit 4
    fi
    rm -f "$raw" "$merged"
    ;;
prefetch|resolve)
    prereq || exit 2
    read_selection || exit 3
    rm -f "$DATA/prefetch-failed.txt"
    if [ "$1" = prefetch ]; then fetch_thumb "$id" & fi
    if ! resolve_core; then
        printf '%s\n' "$id" > "$DATA/prefetch-failed.txt.tmp.$$"
        mv -f "$DATA/prefetch-failed.txt.tmp.$$" "$DATA/prefetch-failed.txt"
        exit 4
    fi
    ;;
invalidate)
    read_selection || exit 3
    slot=$(printf '%s:%s' "$id" "$quality" | cksum | awk '{print $1 % 4}')
    rm -f "$CACHE_DIR/direct-live" "$CACHE_DIR/slot-$slot" "$CACHE_DIR/live-ready" "$CACHE_DIR/live-hls.m3u8" "$DATA/prefetch-ready.txt" "$DATA/stream-url.txt" "$DATA/stream-kind.txt"
    ;;
*) echo 'Usage: youtube-backend.sh home|home-thumbs|thumbs|remember|clear-history|search|more|prefetch|resolve|invalidate|cache-size|clear-cache' >&2; exit 2;;
esac
