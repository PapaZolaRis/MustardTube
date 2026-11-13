# Current Beta 2.4.5 packaging workflow

Use the repository-root `build-package.sh` with the intact, verified
Beta 2.2.7 `.muxapp` as its argument. The script runs source tests, rebuilds
the first-party ARM64 GUI and hotkey helper, retains the baseline's bundled
runtimes, and packages all four themes, the OTA update UI and optional personalization.
It also compiles and runs the host-only preference/reset regression harness.
Beta 2.4.5 passed ARM64 build verification and hardware acceptance on RG35XX Plus and SP (user-reported). Keep Beta 2.4.2 available for rollback. Output files:
`dist/MustardTube-v0.9.11-Beta2.4.5-OTA.muxapp`,
`dist/MustardTube-v0.9.11-Beta2.4.5-Source.zip`, and
`dist/SHA256SUMS.txt`.

The baseline `.muxapp` is **not** part of this repository source archive;
the build is therefore not fully standalone. Rights and corresponding-source
obligations for the bundled third-party runtimes must be resolved before
public binary distribution. This document's older build guidance follows
for historical reference; use the root script for the current release.

---

# Building MustardTube v0.9.11 Beta 2.2.8 (GPL / branding source)

This is the developer build procedure for the source release targeting **muOS
2606.0 Andromeda**, ARM64, on the **RG35XX Plus** and **RG35XX SP**. The
previous Beta 2.2.7 `.muxapp` was hardware-tested on both devices.
Beta 2.2.8 has **not** been compiled or hardware-tested yet.

## Important distinction: installer versus source

This Beta 2.2.8 developer source archive contains project source, documentation, tests and
small artwork. It **does not** contain compiled `mustardtube`, `radio-hotkeys`,
`ffplay-radio`, Node.js, yt-dlp or QuickJS. Do not install the source ZIP using
muOS Archive Manager. End users should install the matching Beta 2.2.8 `.muxapp` **only after**
it has been built, audited and published. Keep the tested Beta 2.2.7 installer as fallback.

The Beta 2.2.8 **root `build-package.sh`** workflow reuses a **validated
Beta 2.2.7 HD Streaming `.muxapp`** to preserve its verified Node/yt-dlp/QuickJS runtime. Rebuilding
only the SDL2 application is not enough for a complete installer.

## Build prerequisites

- A prepared Ubuntu AArch64 cross-compilation VM with `aarch64-linux-gnu-gcc`,
  Ninja, CMake, `make`, `pkg-config`, `zip`, `unzip`, `file` and `readelf`.
- ARM64 SDL2, SDL2_ttf, SDL2_image and OpenSSL development headers/libraries.
- Node.js and Python 3 for the source tests.
- The hardware-tested Beta 2.2.7 HD Streaming `.muxapp` available locally.
- The reviewed source tree for FFmpeg/ffplay. The tested player was built from
  FFmpeg 8.1.3; see `tools/FFPLAY_SOURCE.txt`. Respect third-party licences and
  corresponding-source obligations when distributing compiled software.

## Step 1: Run source tests

From the `SOURCE/` directory:

```bash
bash scripts/test-beta22-source.sh
bash scripts/test-beta226a-ui.sh
bash scripts/test-gpl-migration.sh
```

The suite covers radio process lifecycle, background audio controller,
identity-checked game mixing and restoration, controller shortcuts, Home,
Search, recorded/live video backend handoff, cache, and display geometry.
These are host-side regressions, **not** a substitute for device tests.

## Step 2: Cross-compile the UI and game-audio helper

```bash
bash scripts/build-arm64-ubuntu.sh
bash scripts/build-radio-hotkeys-arm64.sh
```

Outputs:
- `build-arm64-ubuntu/mustardtube` — native SDL2 UI
- `tools/radio-hotkeys` — optional in-game evdev/PipeWire control helper

## Step 3: Build or reuse the ARM64 audio-only player

If `tools/ffplay-radio` is not already present from your audited ARM64 build:

```bash
bash scripts/build-background-player-arm64.sh /absolute/path/to/ffmpeg-8.1.3
```

Validate the compiled player and its linked libraries:

```bash
bash scripts/audit-player-arm64.sh tools/ffplay-radio
```

The static audit checks architecture, dependencies and GLIBC symbol version.
Passing it alone cannot establish compatibility with stock muOS, PipeWire,
playing games, or device-specific power management.

## Step 4: Package using the proven runtime baseline

For the new GPL / branding release use **`build-package.sh` in the
extracted archive root (parent of `SOURCE/`)**, not the historical packaging commands below. This
builder verifies baseline script hashes, rebuilds the ARM64 GUI and hotkey
helper from source,
replaces GUI icons, installs GPLv3 notices and includes the GPL-first-party
source ZIP with the `.muxapp` installer. All bundled third-party source/license
obligations remain your responsibility to audit before publishing.

```bash
cd /path/to/extracted/Beta2.2.8
bash build-package.sh /path/to/MustardTube-v0.9.11-Beta2.2.7-HD-Streaming.muxapp
```

The old experimental instructions below are retained for historical reference.

### Historical Beta 2.2.6a packaging (disabled with GPLv3 license; do not run)

```bash
bash scripts/package-beta22-from-beta21-installer.sh \
  /absolute/path/to/MustardTube-v0.9.11-Beta2.1.muxapp

bash scripts/verify-package.sh \
  dist/MustardTube-v0.9.11-Beta2.2.6a-DualMixer-StaleSessionFix-UI-EXPERIMENTAL-Andromeda-ARM64.muxapp
```

This script deliberately creates the **long-named developer installer** used
for Beta 2.2.6a validation. It checks that the compiled executables are
current and ARM64, copies the existing stable runtime from Beta 2.1, and
checks that its runtime executables remain unmodified.

A previous GitHub release used the shortened external filename
`MustardTube-v0.9.11-Beta2.2.6a.muxapp`. Its **internal installer content is
unchanged** from the developer installer. Use the separately provided release
packaging script to export that filename and generate checksums; do not rename
source files or edit the binary to change the release name.

**Do not use `scripts/package-muxapp.sh` to make the Beta 2.2.6a installer.**
That is a retained historical v0.9.9 packager and is not the current pipeline.

## Step 5: Test the newly built installer on hardware

- Back up installed MustardTube app data and keep the validated previous
  installer as a fallback.
- Validate radio foreground/background playback and the stock Wasabi video
  handoff on both RG35XX Plus and SP.
- In **RADIO+GAME** mode, test game and radio sliders, L2+R2+direction hotkeys,
  hold repeat and slider synchronization with a known game such as PPSSPP.
- Stop radio through the UI and confirm the managed game stream returns to
  100% on a supported game-relaunch workflow. Do not change global master
  audio volume or unrelated streams.
- Test game performance, distorted or missing sound, sleep/lid behavior,
  Wi-Fi interruptions and normal app exit.

## Repository documentation

- `README.md` — current feature overview and installation
- `docs/RELEASE-0.9.11-BETA2.2.6A.md` — tested release scope
- `docs/RELEASE-0.9.11-BETA2.2-RADIO-BACKGROUND-EXPERIMENTAL.md` — historical
  early Beta 2.2 design, **not current behavior**
- `docs/RADIO-VISUALS-DEV.md` — historical radio-artwork work, **not current
  release status**

## Privacy and distribution

Never publish signed stream URLs, personal searches, passwords, Wi-Fi keys,
or runtime data from an installed device. Preserve third-party software and
artwork notices. GPL/LGPL obligations for the bundled FFmpeg-derived player
and other components must be reviewed before redistributing the installer.
