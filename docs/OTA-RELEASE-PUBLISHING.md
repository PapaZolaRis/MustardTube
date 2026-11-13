# MustardTube — GitHub OTA release publishing

Official releases: https://github.com/PapaZolaRis/MustardTube/releases

## Current installation policy

Beta 2.4.5 inherits the Beta 2.4.2 OTA updater and checks for releases automatically at most once per day when enabled
(default on; configurable in Software updates). Checking makes no changes to
the installed software. Downloading requires the user to select **Prepare
verified update**. After preparation, the user exits and selects the package in
**Applications > Archive Manager**. There is no silent auto-install or self-
overwrite. Existing playback, radio and controller code is unchanged.

## Publishing requirements

1. Close contributor-rights and bundled GPL/LGPL corresponding-source issues
   before publicly distributing the GPLv3 installer.
2. Hardware-test the version on RG35XX Plus and SP, including settings/favourites
   across a manual Archive Manager upgrade.
3. Create a GitHub release with a matching tag, e.g. `v0.9.11-beta2.4.5`
   (pre-release) or `v0.9.12` (stable).
4. Upload a **versioned** asset, for example:
   `MustardTube-v0.9.11-Beta2.4.5-OTA.muxapp` and the matching source ZIP.
5. GitHub Releases API must include `digest: "sha256:<64 hex digits>"` for the
   installer. Publish the generated `SHA256SUMS.txt` alongside the package.
6. Never replace earlier MIT releases under an existing tag.
7. Keep a prior working installer available as a rollback package.

## Safety measures

- Only the official public GitHub Releases endpoint is queried. HTTPS and
  expected release asset hostnames and paths are required.
- No different version or digest can be silently substituted between update
  discovery and preparation; a new check is required after release changes.
- Partial downloads are stored under a temporary name, verified for SHA-256,
  length, ZIP CRC and path layout, then atomically renamed into `ARCHIVE`.
- Refuses to overwrite a different archive of the same name.
- Downloads are limited to 250 MiB with free-space buffer and timeouts.
- A separate, bounded user-data backup is placed in
  `ARCHIVE/.mustardtube-recovery/` prior to showing Ready.
- No signed stream URLs or log files are included in recovery backups.
- Installation itself remains an Archive Manager operation; there is no
  automatic transaction or automatic rollback while the app is running.
- A GitHub API SHA-256 digest verifies integrity, not independent maintainer
  identity. Publisher signature verification remains future hardening work.

## Recovery

If the new app fails, reinstall the previously validated `.muxapp` through
Archive Manager. If user settings were lost, the recovery snapshot is stored
in `ARCHIVE/.mustardtube-recovery/` on SD1. Its JSON file contains per-file
SHA-256 digests and is private to the user. It is **not** an official `.muxbak`
and must not be installed with Archive Manager. Restoration should be an
explicit, supervised recovery operation; there is no automatic restore.

For an explicit, supervised restore over SSH (after reinstalling a known-good
MustardTube build), first inspect the hidden backup folder and test on a copy
of your data. Then run, with the correct filename substituted:

```sh
cd /run/muos/storage/application/MustardTube
./bin/node scripts/ota-update.js restore-settings \
  MustardTube-settings-v0.9.11-beta2.4.1-YYYY-MM-DDTHH-MM-SS-sssZ.json
```

The restore command is **not** triggered by any app button. It checks the
backup checksum and restores only allowlisted preference/history files,
including SDL's separate theme settings file. It never restores cached stream
URLs. These recovery instructions assume the original `ARCHIVE` backup remains
on the same SD card.

## Testing

```sh
node scripts/test-ota-update.js
python3 scripts/test-beta241-ui.py
```

The full source packaging script runs these tests. Real GitHub download and
installation must be tested on both devices with a *newer, compatible*
published release. These tests cannot be simulated by merely selecting
`Check for updates` against a repository with no newer release.
