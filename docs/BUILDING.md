# MustardTube Beta 2.2 — developer build using Beta 2.1 runtime

This is a **developer-only** guide for the RG35XX Plus / RG35XX SP running
**muOS 2606.0 Andromeda**. This source cannot be deployed by itself. The
experimental background-radio feature requires an ARM64 ffplay build and
on-device verification of its audio route and process survival during game
launches. Do not replace a working Beta 2.1 install until tests pass.

## Required inputs

- Known-good Beta 2.1 `.muxapp` installer (provided by the project owner).
- This Beta 2.2 source tree.
- Prepared Ubuntu AArch64 cross-compile VM with `aarch64-linux-gnu-gcc`,
  CMake, Ninja, the ARM64 SDL2, SDL2_ttf, SDL2_image and OpenSSL developer
  headers and libraries, plus `node`, `pkg-config`, `zip`, `unzip`, `file` and `readelf`.
- A reviewed source checkout/tarball of FFmpeg that includes `ffplay`; the build
  script accepts its **absolute directory path**, not a binary URL.

## Build on the Ubuntu VM

Run from the source tree:

```sh
bash scripts/test-beta22-source.sh
bash scripts/build-arm64-ubuntu.sh
bash scripts/build-background-player-arm64.sh /absolute/path/to/ffmpeg-source
bash scripts/audit-player-arm64.sh tools/ffplay-radio
bash scripts/package-beta22-from-beta21-installer.sh /absolute/path/to/MustardTube-v0.9.11-Beta2.1.muxapp
bash scripts/verify-package.sh dist/MustardTube-v0.9.11-Beta2.2-Radio-BG-EXPERIMENTAL-Andromeda-ARM64.muxapp
```

The packaging script reuses the runtime from Beta 2.1 and replaces only the
native UI, launch scripts, and necessary new files. It verifies Node, yt-dlp
and QuickJS are unchanged. It refuses to package a missing or non-ARM64 player.

## Compatibility audit

The `audit-player-arm64.sh` script rejects an audio decoder dynamically linked
to `libav*`/`libsw*` libraries that are not bundled by this packaging workflow.
It prints any other dynamic dependencies for verification against **stock
Andromeda**, and checks the highest requested GLIBC symbol version against the
known device baseline (2.44 reported by the test device). Passing this static
audit is not proof that playback works: dynamic libraries, PipeWire audio,
muOS process management and simultaneous game audio require device testing.

## First handheld test

1. Back up the installed Beta 2.1 app and its `data` directory. Install Beta
   2.2 only on a test device/card. Keep the original `.muxapp` for rollback.
2. Play radio normally with **A**. Inspect whether Wasabi actually renders the
   station name or a visualiser. Named M3U metadata is a *request*, not a
   guarantee of a visible title. Confirm direct stream fallback.
3. Press **START** on a station. Confirm the station name appears in the Radio
   page and audio is audible. If there is silence, inspect a redacted log and
   the player dynamic libraries. Do not proceed assuming background audio works.
4. Exit to muOS, start a lightweight GB/SNES game, and check radio plus game
   audio. Then test Pickles / RetroArch and more demanding games separately.
5. Return to MustardTube and press **SELECT** to stop. Check no radio remains.
   Starting a foreground radio or YouTube video should also stop the background
   radio first.
6. Test shutdown, Wi-Fi loss, reconnecting, RG35XX SP lid behaviour and
   long playback sessions. There is no global in-game radio hotkey or guaranteed
   reconnect in this experimental version.

Privacy: do not share signed video URLs or complete radio stream URLs from
logs. Stop/status state stores station name, PID, and binary identity, not
stream credentials.
