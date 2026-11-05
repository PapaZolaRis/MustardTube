# MustardTube v0.9.11: country flags and radio station artwork (development source)

**Status:** source integration and offline tests completed. This is not a released .muxapp. Build and RG35XX Plus/SP hardware validation remain required.

## Country flags

- One pre-generated 512 x 368 RGBA atlas with 249 ISO 3166-1 alpha-2 flag icons, occupying 133,071 bytes on disk (~130 KiB).
- Atlas is decoded once on entering Radio (approximately 736 KiB uncompressed), not one texture per flag.
- 32 x 23 pixel atlas cells; country codes are used as a fallback when a flag is unavailable.
- Artwork is adapted from Twemoji (CC BY 4.0). See `assets/flags/ATTRIBUTION.txt`. Original emoji font is not bundled.

## Station artwork

- Radio Browser `favicon` metadata is optional. Stations without valid logos show a built-in radio tile.
- Logos are fetched **only for a visible page of at most seven stations**, by a separate cancellable Node worker. The UI and audio player never wait for them.
- The logo fetcher requires public HTTPS URLs, follows no redirects, accepts PNG and JPEG only, rejects files over 48 KiB and dimensions larger than 128 x 128, and times out at 2.5 seconds per logo.
- Total on-disk logo cache: 3 MiB and at most 100 entries; downloaded images are shared by station UUID and do not persist as a preloaded collection in RAM.
- No new runtime dependencies or additional audio decoder. Stock Wasabi handles audio playback.
- `Settings -> X Clear Cache` removes disposable logos and directories but **preserves radio favourites** and the saved viewing preferences.

## Tests run off-device

- Node JS country directory / M3U fallback / pagination / `favicon` metadata / lazy logo caching.
- C syntax and native country/favourites/selected station test using the SDK stubs, without asserting real SDL render fidelity.
- Shell launcher / Wasabi handoff / cache clearing / preservation of favourites.
- Existing YouTube backend regression tests.
- Checks of flag atlas size and data/code mapping.

## Still required before distributing a muOS forum beta

1. Compile the ARM64 SDL application in the prepared Ubuntu cross-compile VM.
2. Package it with the updated `scripts/radio-backend.js` and `assets/flags/` directory using `scripts/package-radio-from-v0910-installer.sh`.
3. Test real flags, logos, favourites, country paging and Wasabi radio playback on the RG35XX Plus.
4. Check memory and Wi-Fi usage with a long radio session, and test offline fallback.
5. Finish the agreed video Home categories and Search thumbnail UI. These are **not** included in this Radio visuals patch.

**Compatibility policy:** muOS 2606.0 Andromeda on H700 (RG35XX Plus and RG35XX SP) only. No PortMaster, no extra end-user packages, no unsupported 720p changes.
