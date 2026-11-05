# MustardTube v0.9.5 — Temporary storage reliability fix

**Source candidate only** for muOS 2606.0 Andromeda (RG35XX Plus/SP). Cross-compile on Ubuntu; not yet tested on actual handheld.

## Diagnosis

The single-file PyInstaller yt-dlp creates approximately 88 MB `_MEI*` folders in `/tmp` on each extraction. Repeated, abruptly cancelled video lookups stranded folders until `/tmp` reached 486.5/486.5 MB, causing startup to fail immediately (`[PYI-...:ERROR] Failed to extract...`). Rebooting reset the tmpfs; it was never a persistent YouTube connectivity failure.

## Fixes

- Uses **official, SHA-256-verified unpackaged ARM64 yt-dlp** (`yt-dlp_linux_aarch64.zip`). Its support directory is part of the `.muxapp` so it does not self-extract into `/tmp` on each launch. This is larger on SD storage (~148 MB unpacked), but the previously measured performance-mode startup was 3s vs 4s for single-file and it eliminates this known temp directory exhaustion failure.
- Removes automatic video extraction when merely highlighting search results. Extraction starts **only after opening Details**, and is reused when Play is pressed.
- Preserves bounded 60s resolve/66s UI waiting, Fast 360p default, Y-to-toggle Clear 480p, video details, icon, themes, working X Load More, and Wasabi resume.
- Uses muOS **Application Options → Governor → Performance** for the app; never writes CPU governor settings itself.

## Build from Ubuntu

```bash
cd ~/projects/mustardtube-v0.9.5
mkdir -p tools
# Reuse the QuickJS binary and license from v0.9.4 if present.
if [ -d ../mustardtube-v0.9.4/tools ]; then
  cp -an ../mustardtube-v0.9.4/tools/qjs ../mustardtube-v0.9.4/tools/QUICKJS_LICENSE.txt tools/ 2>/dev/null || true
fi
bash scripts/build-runtime.sh
bash scripts/test-youtube-backend.sh
bash scripts/test-ui.sh
bash scripts/build-arm64-ubuntu.sh
bash scripts/package-muxapp.sh
bash scripts/verify-package.sh
```

Installer: `dist/MustardTube-v0.9.5-Andromeda-ARM64-TEST.muxapp`.

## Before installing on device

Close MustardTube and Wasabi. Ensure no `yt-dlp` or `/tmp/_MEI` process is running. Then remove **only abandoned** `/tmp/_MEI*` directories using the separately supplied guarded cleanup command, or reboot once. Verify free space with `df -h /tmp`. Back up existing installed app with BusyBox-supported `tar -cf` before installing.

## What remains to verify

1. `yt-dlp --version` runs in ~3s in muOS Performance mode (benchmark dependent).
2. Search/Play several different videos, including changes between 360p/480p, without filling `/tmp` again. Check `df -h /tmp` and `find /tmp -maxdepth 1 -name '_MEI*'`.
3. Wasabi video/audio playback and return with MENU+START.
4. Real stream extraction might **still take tens of seconds** because YouTube's extraction/challenge processing was measured at 41s. This update fixes the confirmed temp exhaustion mechanism, not all sources of long playback preparation.
5. If installation extracts over an older version, old files under `bin/` might remain but are unused. For a clean replacement, back up the old folder and move it aside before extracting, provided the backup completed successfully.
