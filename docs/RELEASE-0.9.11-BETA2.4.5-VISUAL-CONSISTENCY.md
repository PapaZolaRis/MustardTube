# MustardTube v0.9.11 Beta 2.4.5 — Visual consistency (source candidate)

Target: muOS 2606.0 Andromeda, RG35XX Plus and RG35XX SP.

- Separate loading, missing-artwork and playback visuals throughout Home, Search Results and Video Details.
- Replace old solid yellow play-circle placeholders with theme-aware film-frame icons.
- Show three theme-colored dots for loading Home discovery and empty Radio directory refreshes, refreshing at most once per 420 milliseconds while loading.
- Preserve Beta 2.4.4's small dark/outlined preview play control when real video artwork is present.
- Keep all existing layout geometries, text/focus controls, six main tabs, themes and user preferences.
- No new textures, fonts, network requests, threads or external dependencies; media, OTA and radio backend scripts unchanged apart from the current OTA version marker.

Development verification does not replace ARM64 build and real-device visual testing.
Prior beta installers remain available for rollback. Public source/binary release requires contributor and bundled-dependency provenance review.
