#!/usr/bin/env bash
# Historical packaging workflow. It cannot assemble a compliant Beta 2.2.8
# build with matching source and GPLv3 notices. Use ../build-package.sh.
if grep -Fq 'Version 3, 29 June 2007' "$(dirname "$0")/../LICENSE"; then
  echo 'Historical packaging flow disabled for GPLv3 builds. Run the parent build-package.sh.' >&2
  exit 2
fi
# Repackage the original working v0.9.9 installer with ONLY a rebuilt native UI
# executable plus the user's notice. Original launcher/backends/runtimes retained.
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT="$PWD"
ORIGINAL_INSTALLER="${1:-}"
if [[ -z "$ORIGINAL_INSTALLER" || ! -f "$ORIGINAL_INSTALLER" ]]; then
  echo 'Usage: bash scripts/package-about-from-original-installer.sh /absolute/path/to/ORIGINAL-v0.9.9.muxapp' >&2
  exit 2
fi
ORIGINAL_INSTALLER="$(readlink -f "$ORIGINAL_INSTALLER")"
NEW_BINARY="$ROOT/build-arm64-ubuntu/mustardtube"
[[ -s "$NEW_BINARY" ]] || { echo 'First run: bash scripts/build-arm64-ubuntu.sh' >&2; exit 2; }
file "$NEW_BINARY" | grep -q 'ARM aarch64' || { echo 'Rebuilt binary is not ARM64' >&2;exit 2; }
unzip -tq "$ORIGINAL_INSTALLER" >/dev/null || { echo 'Original archive failed integrity test' >&2;exit 2; }
mkdir -p "$ROOT/dist"
OUT="$ROOT/dist/MustardTube-v0.9.9-PapaZolaRis-About-Legal-Andromeda.muxapp"
[[ "$OUT" != "$ORIGINAL_INSTALLER" ]] || { echo 'Refusing to overwrite original' >&2; exit 2; }
TMP="$(mktemp -d)";trap 'rm -rf "$TMP"' EXIT
unzip -q "$ORIGINAL_INSTALLER" -d "$TMP"
[[ -x "$TMP/MustardTube/bin/mustardtube" && -f "$TMP/MustardTube/mux_launch.sh" ]] || { echo 'Wrong package structure' >&2;exit 2; }
cp "$NEW_BINARY" "$TMP/MustardTube/bin/mustardtube"
chmod 755 "$TMP/MustardTube/bin/mustardtube"
cp "$ROOT/docs/PAPAZOLARIS-USER-NOTICE.txt" "$TMP/MustardTube/licenses/USER_NOTICE.txt"
(cd "$TMP" && zip -q -r "$OUT" MustardTube)
unzip -tq "$OUT" >/dev/null
# Catch unexpected changes to the original supervisor or YouTube backend.
for part in mux_launch.sh scripts/youtube-backend.sh; do
  if ! cmp -s <(unzip -p "$ORIGINAL_INSTALLER" "MustardTube/$part") \
               <(unzip -p "$OUT" "MustardTube/$part"); then
    echo "Unexpected modification: $part" >&2; exit 1
  fi
done
printf 'Built %s\n' "$OUT"
echo 'Original launcher/backend preserved. Device validation still required.'
