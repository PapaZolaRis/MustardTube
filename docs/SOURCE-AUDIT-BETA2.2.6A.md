# Source archive audit — Beta 2.2.6a

The source ZIP uploaded for review contained the current Beta 2.2.6a
application/mixer/hotkey/controller logic. All eight of the corrected source,
test and packaging files shipped by the Beta 2.2.6a patch matched byte-for-byte.
The host-side `scripts/test-beta22-source.sh` and `scripts/test-game-volume.sh`
passed against the original uploaded source archive.

Only repository-maintenance changes were made for this reviewed source package:

- Updated README to describe Beta 2.2.6a rather than the earlier Beta 2.2
  source-only experiment.
- Updated `docs/BUILDING.md` to include building `radio-hotkeys` and to
  distinguish the source ZIP from the compiled `.muxapp` installer.
- Added explicit historical-status notes to early experiment documents.
- Fixed `.gitignore`, which previously ignored all `*.png` files and could
  prevent a fresh repository from tracking required artwork/fixtures.
- Removed a developer-specific default LAN IP from the optional benchmark
  helper; callers now supply their own device address.
- Replaced obsolete test `scripts/test-beta223a-ui.sh`, which asserted old
  Beta 2.2.3b release strings and failed even on correct Beta 2.2.6a code,
  with `scripts/test-beta226a-ui.sh` and added it to the main source test suite.

**No current runtime C, JavaScript controller, launcher, or cross-build/package
implementation was changed.** The separately distributed tested `.muxapp`
installer is not modified by this source-only documentation/test cleanup.

The source ZIP intentionally excludes precompiled runtime/player binaries,
build directories, and device data. Cross-building and packaging require
additional developer inputs described in `docs/BUILDING.md`.

This audit confirms alignment with the provided Beta 2.2.6a patch, **not** an
independent audit of a remote GitHub HEAD or a reproducible binary build.
