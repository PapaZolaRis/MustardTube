# MustardTube v0.9.11 Beta 2.2.6a

Target: **muOS 2606.0 Andromeda**, ARM64, Anbernic RG35XX Plus / RG35XX SP.

## Validated capabilities

- Built-in internet radio and background radio during gameplay
- Separate Radio Volume and Game Volume controls in Now Playing
- RADIO+GAME shortcut mode: L2+R2+Left/Right for radio, L2+R2+Up/Down for game
- Single 5% steps and faster press-and-hold volume adjustments
- Slider and in-game shortcut synchronization
- Managed game audio stream resets to 100% after radio stops, including
  identity-checked same-emulator recovery when applicable
- Improved stale game-session handling and separated player-status text
- RG35XX Plus and RG35XX SP device acceptance tests reported successful

## Important scope and constraints

The 100% reset applies only to a game audio stream MustardTube manages. It does
not set the muOS master output to 100% or change unrelated application streams.
Keep RADIO+GAME enabled for automatic Game Volume mixing. Deferred recovery is
bounded and may not succeed if a game restarts after its recovery window.

This GitHub source archive excludes the bundled compiled Node.js, yt-dlp,
FFplay, and other third-party runtime binaries shipped in the .muxapp.
Retain licenses/notices in the installer and review third-party distribution
requirements before publishing public release assets.

This release was packaged from a locally tested Ubuntu source tree. The `.muxapp`
file is a byte-for-byte copy of the validated long-named Beta 2.2.6a installer;
only its **external filename** was shortened.
