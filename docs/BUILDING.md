# Building MustardTube v0.9.11 Beta 2

This repository is the source tree for the Beta 2 community test build.
The published installer includes separate ARM64 runtime components that are
**not** committed to GitHub. Building from source is a developer task.

## Target

- ARM64 / Cortex-A53 (`aarch64`), muOS 2606.0 Andromeda
- Native SDL2, SDL2_ttf and SDL2_image
- Node.js and yt-dlp runtime used by the original MustardTube distribution
- Wasabi supplied by the target firmware; it is not embedded in this repo

## Native executable

On an Ubuntu host with the aarch64 Linux cross-compiler, cross-target SDL2
headers/pkg-config files, CMake and Ninja installed:

```bash
bash scripts/test-youtube-backend.sh
node scripts/test-radio-backend.js
bash scripts/build-arm64-ubuntu.sh
```

These commands depend on the host's build environment and may require
additional preparation. The release build used an alternative cross-link
procedure; **the script above is not claimed to reproduce that binary
bit-for-bit**. Always test any newly compiled binary on the handheld.

## Packaging with the existing v0.9.10 runtime

The packaging script below takes an existing **known-working v0.9.10**
installer and replaces the relevant application binary and scripts while
retaining the bundled runtime.

```bash
bash scripts/package-radio-from-v0910-installer.sh \
  /absolute/path/to/MustardTube-v0.9.10-Explore-Home-REISSUED.muxapp
```

It expects `build-arm64-ubuntu/mustardtube` to already exist. It also copies
radio assets and launcher integration. Inspect the finished package and its
third-party licences, and run platform tests before distribution.

**Do not use `scripts/package-muxapp.sh` for the Beta 2 release:** that file
is retained as a legacy reference from v0.9.9 and produces an outdated package.

## Important checks

- The video backend's 360p/480p route must not change unexpectedly.
- Experimental recorded 720p must remain clearly labelled with no seek.
- Radio favourites must survive Clear Cache.
- Sensitive signed media URLs must not be written to publicly shared logs.
- All external playlist and station URLs should be treated as untrusted.
- Validate the resulting `.muxapp` on actual RG35XX Plus/SP hardware.
