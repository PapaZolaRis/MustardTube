# MustardTube v0.9.11 Beta 2.4.3 — Personalization (candidate)

Source-only candidate. No ARM64 build or device validation is claimed here.
Built on the hardware-validated Beta 2.4.2 source without changes to streaming,
Internet Radio, OTA downloader, launcher, theme assets or game-audio mixing.

## New UI options

- Navigation layout: Vertical sidebar (default), Horizontal top tabs, Compact sidebar.
- Card density: Standard (four thumbnails/results per page), Compact (six text-first search results per page). Home retains four per batch.
- Default startup page: Home, Search, Recent, Radio.
- Content text size: Small, Normal (default), Large.
- Two-press appearance reset: does not delete favorites, watch/search history, radio/game volumes, caches or OTA settings.

Settings > Personalization; UP/DOWN select; LEFT/RIGHT or A changes; B returns.
L1/R1 main tab navigation and all prior shortcuts remain. Preference changes
persist in SDL's `settings.ini` and are included in the existing OTA snapshot.

## Testing

- Prior-release static tests for layouts, themes, streaming and OTA are retained.
- `python3 scripts/test-beta243-personalization.py`: structure and protected media hash invariants.
- `bash scripts/test-beta243-personalization.sh`: compiles the actual C preference functions with host SDL stubs and tests persistence, malformed settings, geometry mapping and data-safe reset.
- On-device tests required: 3 navigation layouts × 4 themes; 2 card densities; 3 text sizes; startup pages; controller and mouse/touch focus; video/radio/OTA; RG35XX SP lid close/resume.

## Compatibility

Only muOS 2606.0 Andromeda on RG35XX Plus/SP is targeted. No Shorts mode,
automatic archive installation, new dependencies or playback-engine changes.
Keep the hardware-tested Beta 2.4.2 `.muxapp` as rollback until both devices pass.

## Publication

The build still requires the validated Beta 2.2.7 `.muxapp` for runtime components.
Contributor-rights and GPL/LGPL bundled-source obligations remain unresolved;
review before public binary release. Do not repoint the existing Beta 2.4.2 tag.
