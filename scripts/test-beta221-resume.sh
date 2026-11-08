#!/usr/bin/env bash
# Host-only regression: active radio resumes directly to native Now Playing.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
app="$tmp/MustardTube"
mkdir -p "$app/bin" "$app/scripts" "$app/data"
: > "$app/scripts/background-radio.js"
cat > "$app/bin/node" <<'NODE'
#!/bin/sh
case "$2" in
  status) if [ -f "$MUSTARDTUBE_APP_DIR/data/live" ]; then echo 'Test Radio';else exit 1;fi;;
  start) touch "$MUSTARDTUBE_APP_DIR/data/live";;
  stop) rm -f "$MUSTARDTUBE_APP_DIR/data/live";;
esac
NODE
cat > "$app/bin/mustardtube" <<'APP'
#!/bin/sh
printf '%s\n' "$*" >> "$MUSTARDTUBE_APP_DIR/data/launch-args.txt"
n=$(cat "$MUSTARDTUBE_APP_DIR/data/launch-index.txt" 2>/dev/null || echo 0)
n=$((n+1));echo "$n" > "$MUSTARDTUBE_APP_DIR/data/launch-index.txt"
if [ -r "$MUSTARDTUBE_APP_DIR/data/test-background-start" ] && [ "$n" -eq 1 ];then exit 84;fi
exit 0
APP
chmod +x "$app/bin/node" "$app/bin/mustardtube"
sed -e 's@^\. /opt/muos/script/var/func.sh@SETUP_APP() { :; }@' \
  -e "s@^APP_DIR=.*@APP_DIR=\"$app\"@" \
  packaging/mux_launch.sh > "$tmp/launcher.sh"
# Existing live radio must bypass Home on app relaunch.
touch "$app/data/live"
sh "$tmp/launcher.sh"
grep -qF -- '--show-radio-now' "$app/data/launch-args.txt"
[[ "$(wc -l < "$app/data/launch-args.txt")" == 1 ]]
# Stale metadata without a validated active radio must not force Now Playing.
rm -f "$app/data/live" "$app/data/launch-args.txt"
sh "$tmp/launcher.sh"
! grep -qF -- '--show-radio-now' "$app/data/launch-args.txt"
# Pressing START from station directory (background exit 84) now routes to Now Playing.
rm -f "$app/data/launch-args.txt" "$app/data/launch-index.txt"
touch "$app/data/test-background-start"
sh "$tmp/launcher.sh"
[[ "$(wc -l < "$app/data/launch-args.txt")" == 2 ]]
tail -n 1 "$app/data/launch-args.txt" | grep -qF -- '--show-radio-now'
echo 'PASS: live-radio app relaunch returns to Now Playing; stopped radio opens Home; background start opens Now Playing'
