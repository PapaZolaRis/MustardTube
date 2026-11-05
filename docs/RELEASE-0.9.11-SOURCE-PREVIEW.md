# MustardTube v0.9.11 — Source preview (not a public installer)

This is a staging-only source revision, based on the verified v0.9.10 cache-maintenance test build.

- Rename Style tab to Settings in native C UI.
- Add Appearance and Storage & Cache section labels.
- Keep existing theme selection and X: Clear Cache controls.
- Point users to native muOS Application Options for governor selection.
- Retain original Wasabi playback, Home discovery, search, 720p recorded experimental behaviour and About / Legal text.
- DO NOT claim Internet Radio, offline downloads, or new Search thumbnail support is included.
- Radio is an approved direction for future design, not implemented yet.

**Status:** ARM64 compilation and handheld validation still required.

Build on a prepared Ubuntu AArch64 cross-compile VM with:
`bash scripts/build-arm64-ubuntu.sh`. Do not publish as a stable release before RG35XX Plus verification.
