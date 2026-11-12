# Bundled component and license audit (Beta 2.2.7 source + installer)

## Source archive

- `assets/flags/country-atlas.png`: based on Twemoji country flags, CC BY 4.0;
  attribution in `assets/flags/ATTRIBUTION.txt` must remain available.
- `tools/FFPLAY_SOURCE.txt`: identifies FFmpeg 8.1.3 as the bundled `ffplay-radio`
  source baseline. `FFPLAY_GPL_LICENSE.txt` and `FFPLAY_LGPL_LICENSE.txt`
  are preserved, but their presence **does not establish** how the included
  binary was configured or which component licenses actually apply.
- `assets/media/mustardtube-av-test.mp4`: copyright/redistribution origin
  not documented in the supplied source snapshot; verify before publication.
- `assets/branding/`: icons cropped from the maintainer-approved artwork.
  Confirm distribution rights for the source concept if necessary.

## Bundled binary `.muxapp` baseline

- `bin/ffplay-radio`: independent FFmpeg-derived player; verify exact build
  configuration (`--enable-gpl`, linked libraries, external codecs), complete
  matching source, build options and required license notices. The FFmpeg
  license text files do not establish binary compliance by themselves.
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

A matching FFmpeg/ffplay source package and build/configuration evidence
are **not included** in the supplied source ZIP. The `.muxapp` archive
contains FFmpeg license notices but not the entire player source. Other
bundled runtimes must also be audited against matching source and notices.
The actual Git history is unavailable for contributor rights verification.
**Do not mark distribution compliance as complete until verified.**

Do not edit or relicense third-party binaries just to change displayed text.
