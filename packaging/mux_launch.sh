#!/bin/sh
# HELP: MustardTube - YouTube videos via stock Wasabi
# GRID: MustardTube
# ICON: mustardtube
. /opt/muos/script/var/func.sh
APP_BIN="mustardtube"
SETUP_APP "$APP_BIN" ""
APP_DIR="/run/muos/storage/application/MustardTube"
cd "$APP_DIR" || exit 1
mkdir -p data
chmod 700 data 2>/dev/null || true
LOG="$APP_DIR/data/launch.log"
WASABI="/opt/muos/frontend/muxmedia"
TEST_VIDEO="$APP_DIR/assets/media/mustardtube-av-test.mp4"
RADIO_CTL="$APP_DIR/scripts/background-radio.js"
RADIO_NODE="$APP_DIR/bin/node"
MUSTARDTUBE_APP_DIR="$APP_DIR"
MUSTARDTUBE_DATA_DIR="$APP_DIR/data"
export MUSTARDTUBE_APP_DIR MUSTARDTUBE_DATA_DIR
if [ -r /opt/muos/share/font/internal/Inter.ttf ]; then
    MUSTARDTUBE_FONT="/opt/muos/share/font/internal/Inter.ttf"
    export MUSTARDTUBE_FONT
fi
resume=""
while :; do
    # Refresh the small status marker; the controller verifies PID identity
    # before trusting an older background state file.
    if [ -x "$RADIO_NODE" ] && [ -r "$RADIO_CTL" ]; then
        "$RADIO_NODE" "$RADIO_CTL" status > "$APP_DIR/data/radio-background-name.txt" 2>/dev/null || \
            : > "$APP_DIR/data/radio-background-name.txt"
    fi
    # A newly opened MustardTube session must return to the already-running
    # radio, not Home.  Only a PID-validated, successful status response can
    # select this route; stale station metadata never does.
    if [ -z "$resume" ] && [ -s "$APP_DIR/data/radio-background-name.txt" ]; then
        resume=radio-now
    fi
    case "$resume" in
    style) ./bin/mustardtube --assets "$APP_DIR/assets" --show-style >> "$LOG" 2>&1;;
    radio) ./bin/mustardtube --assets "$APP_DIR/assets" --show-radio >> "$LOG" 2>&1;;
    radio-now) ./bin/mustardtube --assets "$APP_DIR/assets" --show-radio-now >> "$LOG" 2>&1;;
    player) ./bin/mustardtube --assets "$APP_DIR/assets" --resume-player >> "$LOG" 2>&1;;
    results) ./bin/mustardtube --assets "$APP_DIR/assets" --show-results >> "$LOG" 2>&1;;
    *) ./bin/mustardtube --assets "$APP_DIR/assets" >> "$LOG" 2>&1;;
    esac
    result=$?
    case "$result" in
    81)
        sh "$APP_DIR/scripts/youtube-backend.sh" clear-cache >> "$LOG" 2>&1 || true
        resume=style
        ;;
    83) # Privacy: local video viewing-history reset, separate from disposable cache.
        sh "$APP_DIR/scripts/youtube-backend.sh" clear-history >> "$LOG" 2>&1 || true
        resume=style
        ;;
    78) # Search requested. No competing SDL renderer while networking.
        sh "$APP_DIR/scripts/youtube-backend.sh" search >> "$LOG" 2>&1 || true
        resume=results
        ;;
    80) # Load the next 8 YouTube results without discarding existing results.
        sh "$APP_DIR/scripts/youtube-backend.sh" more >> "$LOG" 2>&1 || true
        resume=results
        ;;
    77) # YouTube video selected.
        if sh "$APP_DIR/scripts/youtube-backend.sh" resolve >> "$LOG" 2>&1; then
            if [ -x "$WASABI" ] && [ -f "$APP_DIR/data/stream-url.txt" ]; then
                IFS= read -r stream < "$APP_DIR/data/stream-url.txt" || true
                stream_kind=$(cat "$APP_DIR/data/stream-kind.txt" 2>/dev/null || echo vod)
                rm -f "$APP_DIR/data/stream-url.txt" "$APP_DIR/data/stream-kind.txt"
                title='YouTube video'
                if [ -r "$APP_DIR/data/selected.txt" ]; then
                    title=$(sed -n '2p' "$APP_DIR/data/selected.txt")
                fi
                # A foreground video must not overlap a background radio station.
                # Stop by validated player identity; do not terminate unrelated audio.
                if [ -x "$RADIO_NODE" ] && [ -r "$RADIO_CTL" ]; then
                    "$RADIO_NODE" "$RADIO_CTL" stop >/dev/null 2>&1 || true
                fi
                # Never log the signed stream URL, which could contain private tokens.
                started=$(date +%s)
                echo "player_invoked_at=$started" >> "$APP_DIR/data/timing.log"
                player_rc=0
                case "$stream_kind" in
                    live|vod_hls)
                        # Only trusted backend-produced private HLS playlists.
                        # Recorded HLS uses Wasabi's ordinary VOD mode so seeking
                        # may remain available; live HLS uses --live.
                        case "$stream" in
                            /tmp/mustardtube-streams-*/live-hls.m3u8)
                                if [ -f "$stream" ]; then
                                    if [ "$stream_kind" = live ]; then
                                        "$WASABI" --live --title "$title" "$stream" >/dev/null 2>&1 || player_rc=$?
                                    else
                                        "$WASABI" --title "$title" "$stream" >/dev/null 2>&1 || player_rc=$?
                                    fi
                                else player_rc=4; fi;;
                            *) player_rc=4;;
                        esac
                        rm -f "$stream" "${stream%/*}/live-ready";;
                    *) "$WASABI" --title "$title" "$stream" >/dev/null 2>&1 || player_rc=$?;;
                esac
                elapsed=$(( $(date +%s)-started ))
                echo "player_returned duration=${elapsed}s exit=$player_rc stream=$stream_kind" >> "$APP_DIR/data/timing.log"
                # Optional local discovery interest; never stores signed URLs.
                if [ "$elapsed" -ge 5 ]; then
                    sh "$APP_DIR/scripts/youtube-backend.sh" remember >/dev/null 2>&1 || true
                fi
                # A failed or very short launch must not reuse a bad signed URL.
                # Do not auto-relaunch: the user may have deliberately exited.
                if [ "$stream_kind" = live ] || [ "$stream_kind" = vod_hls ] || [ "$player_rc" -ne 0 ] || [ "$elapsed" -lt 5 ]; then
                    sh "$APP_DIR/scripts/youtube-backend.sh" invalidate >/dev/null 2>&1 || true
                fi
            fi
        fi
        resume=player
        ;;
    84) # Experimental audio-only background mode; Wasabi remains foreground-only.
        if [ -x "$RADIO_NODE" ] && [ -r "$RADIO_CTL" ]; then
            if "$RADIO_NODE" "$RADIO_CTL" start >> "$LOG" 2>&1; then
                printf '%s\n' 'Background radio requested. Leave MustardTube to launch a game.' > "$APP_DIR/data/radio-background-notice.txt"
            else
                printf '%s\n' 'Background radio unavailable; check bundled player and diagnostics.' > "$APP_DIR/data/radio-background-notice.txt"
            fi
        else
            rm -f "$APP_DIR/data/radio-selected.txt"
            printf '%s\n' 'Background radio service missing from this installer.' > "$APP_DIR/data/radio-background-notice.txt"
        fi
        resume=radio-now
        ;;
    86) # Native foreground radio: same single ffplay stream, but resume SDL Now Playing.
        if [ -x "$RADIO_NODE" ] && [ -r "$RADIO_CTL" ]; then
            if "$RADIO_NODE" "$RADIO_CTL" start >> "$LOG" 2>&1; then
                printf '%s\n' 'Radio is running. Use START to return to games.' > "$APP_DIR/data/radio-background-notice.txt"
            else
                printf '%s\n' 'Radio failed to start. Press B to choose another station.' > "$APP_DIR/data/radio-background-notice.txt"
            fi
        else
            rm -f "$APP_DIR/data/radio-selected.txt"
            printf '%s\n' 'Built-in radio player unavailable on this device.' > "$APP_DIR/data/radio-background-notice.txt"
        fi
        resume=radio-now
        ;;
    87) # Stop from Now Playing; return to native screen rather than radio directory.
        if [ -x "$RADIO_NODE" ] && [ -r "$RADIO_CTL" ]; then
            "$RADIO_NODE" "$RADIO_CTL" stop >> "$LOG" 2>&1 || true
        fi
        printf '%s\n' 'Radio stopped.' > "$APP_DIR/data/radio-background-notice.txt"
        resume=radio-now
        ;;
    85) # Explicit background stop; no global game hotkey hijacking.
        if [ -x "$RADIO_NODE" ] && [ -r "$RADIO_CTL" ]; then
            "$RADIO_NODE" "$RADIO_CTL" stop >> "$LOG" 2>&1 || true
        fi
        printf '%s\n' 'Background radio stopped.' > "$APP_DIR/data/radio-background-notice.txt"
        resume=radio
        ;;
    82) # Foreground Wasabi radio; preserve video playback paths.
        radio_file="$APP_DIR/data/radio-selected.txt"
        title='Internet Radio'
        stream=''
        if [ -r "$radio_file" ]; then
            title=$(sed -n '1p' "$radio_file" | cut -c 1-110)
            stream=$(sed -n '2p' "$radio_file")
        fi
        rm -f "$radio_file"
        case "$stream" in
            http://*|https://*)
                if [ -x "$WASABI" ]; then
                    # Avoid overlapping game/background radio with foreground Wasabi.
                    if [ -x "$RADIO_NODE" ] && [ -r "$RADIO_CTL" ]; then
                        "$RADIO_NODE" "$RADIO_CTL" stop >/dev/null 2>&1 || true
                    fi
                    # A named M3U can expose station metadata to Wasabi's audio UI.
                    # Do not pass the station's URL to logging or a shell eval.
                    title=$(printf '%s' "$title" | tr -d '\r\n' | cut -c 1-105)
                    playlist="$APP_DIR/data/radio-now-playing.m3u"
                    umask 077
                    printf '#EXTM3U\n#EXTINF:-1,%s\n%s\n' "$title" "$stream" > "$playlist"
                    if ! "$WASABI" --live --title "$title" "$playlist" >/dev/null 2>&1; then
                        # Preserve compatibility if this Wasabi build rejects M3U.
                        "$WASABI" --live --title "$title" "$stream" >/dev/null 2>&1 || \
                            echo 'Internet radio: player returned a nonzero code.' >> "$LOG"
                    fi
                    rm -f "$playlist"
                fi
                ;;
            *) echo 'Internet radio: invalid station link.' >> "$LOG";;
        esac
        resume=radio
        ;;
    79) # Existing local media test, retained for troubleshooting.
        if [ -x "$WASABI" ] && [ -f "$TEST_VIDEO" ]; then
            "$WASABI" --title 'MustardTube A/V Test' "$TEST_VIDEO" >> "$LOG" 2>&1 || true
        fi
        resume=player
        ;;
    *)
        # Remove any short-lived HLS URLs when the user exits the app.
        live_tmp="/tmp/mustardtube-streams-$(id -u)"
        rm -f "$live_tmp/live-ready" "$live_tmp/live-hls.m3u8" \
          "$APP_DIR/data/stream-kind.txt"
        exit "$result";;
    esac
done
