# Bundled component and license audit (Beta 2.4.5 RC1)

## Source archive

- `assets/flags/country-atlas.png`: based on Twemoji country flags, CC BY 4.0;
  attribution in `assets/flags/ATTRIBUTION.txt` must remain available.
- `tools/FFPLAY_SOURCE.txt`: identifies FFmpeg 8.1.3 as the bundled
  `ffplay-radio` source baseline. The device-executed binary has been
  checked using `-buildconf` and `-L`. It reports LGPL 2.1 or later;
  neither `--enable-gpl` nor `--enable-nonfree` appears in its configuration.
  Matching source availability, build instructions, and all applicable
  redistribution obligations still require final verification.
- `assets/media/mustardtube-av-test.mp4`: replaced for Beta 2.4.5 RC1
  with a synthetic 10-second video/audio diagnostic generated using FFmpeg
  testsrc2 and sine filters. No external footage or sound recording is used.
  See `assets/media/AV-TEST-PROVENANCE.md` for the generation parameters
  and SHA-256 checksum.
- `assets/branding/`: icons cropped from the maintainer-approved artwork.
  Confirm distribution rights for the source concept if necessary.

## Bundled binary `.muxapp` baseline

- `bin/ffplay-radio`: independent FFmpeg 8.1.3 player. Its device SHA-256
  is c309cfbcc4b02208384ea944564802ae0a35e3ec9ed9c1e8d10d8b2e12a8d86b.
  Its reported license is LGPL 2.1 or later. Build configuration and
  shared-library dependencies were inspected on the RG35XX Plus.
  Corresponding source, build instructions, license notices, and any
  applicable relinking obligations remain subject to final review.
- `bin/yt-dlp` and `bin/_internal/`: bundled onedir runtime, including Python
  packages and native libraries. Retain `YT_DLP_THIRD_PARTY_LICENSES.txt`,
  `_internal/THIRD_PARTY_LICENSES.txt`, metadata, and all bundled notices;
  verify exact corresponding source and dependencies where required.
- `bin/node` and `bin/qjs`: retain `NODE_LICENSE.txt` and
  `QUICKJS_LICENSE.txt`; verify their exact licensing/provenance and that
  the supplied binaries match the documented upstream sources.
- `bin/radio-hotkeys` and `bin/mustardtube`: application-built binaries;
  rebuild from this release's source, not previous MIT release objects.
- Wasabi: system-provided muOS player, not a MustardTube-bundled binary.
- SDL2, SDL2_ttf, SDL2_image, OpenSSL and other shared libraries: check
  whether they are system-provided or bundled and respect each license.
- No bundled font file is present in the supplied source snapshot; the GUI
  discovers a system font at runtime.

## Release blockers

The matching FFmpeg 8.1.3 source archive has been obtained in the
maintainer's release-audit workspace, and the compiled binary's reported
configuration and license have been verified on hardware. The source
archive is not yet included in the release deliverables.

Complete corresponding-source availability, applicable relinking
requirements, and third-party notices must be reviewed for all bundled
components, including the yt-dlp onedir runtime and its native dependencies.
Branding and contributor distribution rights also require final confirmation.

**Do not mark distribution compliance as complete until verified.**

Do not edit or relicense third-party binaries just to change displayed text.
