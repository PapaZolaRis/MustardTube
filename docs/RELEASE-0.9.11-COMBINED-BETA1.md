# MustardTube v0.9.11 Combined Beta 1 — muOS 2606.0 Andromeda

Developed by **PapaZolaRis**. Unofficial community application; experimental release candidate, **not yet hardware validated**.

## Target devices

Anbernic RG35XX Plus and RG35XX SP (Allwinner H700), running **muOS 2606.0 Andromeda**. No PortMaster, Google sign-in, API key, or manually installed system packages. Internet/Wi-Fi required for streaming and new discovery data. Stock muOS Wasabi is used for playback.

## Implemented (in the compiled .muxapp)

- Six global tabs: **Home, Search, Recent, Radio, Settings, About**.
- Home discovery categories: **Explore, Popular, For You, Music, Gaming, Tech**. The separate category cache loads only the selected category; four real-video cards at a time. **Popular is search-based, not YouTube's official chart**. For You uses the title of the most recently viewed video to suggest broadly similar videos; no YouTube account profile or machine-learning engine.
- Search shows four actual video thumbnails at a time, using the same card style as Home. **X** can add more results repeatedly, up to 40. Image loading runs asynchronously; unavailable images show the lightweight play icon.
- Radio: browse stations by country, station lists paged in batches of 20 (up to 80 per country), favourite up to 32 stations, cached local flag atlas, optional logos loaded only for the visible seven stations with a ~3 MiB icon-cache cap, play via stock Wasabi. Radio Browser directory primary, individual country-specific M3U fallback. Station playback depends on actual stream/player compatibility.
- Settings: Appearance (Midnight/Forest/Paper), **X Clear Cache** (temporary data only), **Y Clear Viewing History** (optional local personalisation data), governor information. Neither action deletes radio favourites or manually saved media.
- Existing 360p and 480p recorded playback paths, plus **experimental recorded 720p (no seek)**. Live video seeking is not supported. Wasabi remains the video and audio player.
- About/Legal retains PapaZolaRis attribution and as-is disclaimer.

## Quick installation

1. Back up your existing `MustardTube` app folder or at least its `data` directory before updating.
2. Copy the `.muxapp` to your SD card's `ARCHIVE` directory (or the folder shown by muOS Archive Manager).
3. Open **Applications -> Archive Manager** in muOS and install the archive.
4. Return to the **Applications** list and launch **MustardTube**; ensure Wi-Fi works.
5. If application loading is slow, optionally change **Applications -> Application Options -> MustardTube -> Governor -> Performance**. The installer does not change the system governor, and Performance can increase battery use and heat.

## Controls

| Screen | Buttons |
|---|---|
| Global tabs | LB / RB to change tabs; B to return to Home |
| Home | Left/Right or Y changes discovery category; Up/Down selects video; A opens details; X refreshes only that category |
| Search | D-pad navigates keyboard; A types; Start submits search |
| Search results | Up/Down selects; A opens video details; X loads more results; B returns to Search |
| Video details | A plays via Wasabi; Y cycles 360p, 480p, experimental 720p; B backs out |
| Radio countries | Up/Down selects country; A opens it; X refreshes directory; Y opens favourites |
| Radio stations | Up/Down selects; A starts station; Y toggles favourite; X loads more; B returns to country list |
| Radio favourites | A plays; Y removes favourite; B returns to countries |
| Settings | A applies selected theme; X clears temporary cache; Y clears local viewing history |
| About | B returns Home |
| Wasabi | MENU + START was the previously verified return-to-app shortcut; confirm on current firmware |

## Expected beta limitations

- Country/station listings depend on external directory availability; cached data may appear when offline.
- Internet Radio playback on physical RG35XX hardware has **not yet been validated**. Some stations may be down, redirect unexpectedly, use unsupported audio codecs, or fail to open in Wasabi.
- Search thumbnails may take a few seconds to appear; failed downloads show an icon rather than delaying navigation.
- Home `Popular` is discovery by search, not the official YouTube most-popular API chart.
- For You stores a small list of up to 12 video IDs/titles locally. When For You is opened, the latest title is included in a YouTube discovery query. The Settings history-clearing command deletes these entries; cache clearing deliberately preserves them.
- Recorded 720p is experimental and does **not** support seeking; live videos also do not seek. Recorded 360p/480p seeking works on compatible streams.
- No offline-download feature. No promised Netflix/Disney DRM support.
- Automatic governor assignment during installation is **not** enabled.
- ARM64 build and offline regressions passed, but device-side smoke testing is required before this can be called stable or posted as a generally working release.

## Screenshots

On muOS, press **L2 + R2 + X** together while the relevant MustardTube screen is visible. Find saved images under `MUOS/screenshot` on the configured card. Alternatively use the muOS Web Dashboard **Remote View** and save a capture. Suggested captures: Home categories, Search thumbnails, Radio country list with flags, station list with logos, Settings and About.

## Reporting issues

Please report: RG35XX model (Plus/SP), firmware version, screen/tab, controller steps, expected/actual behaviour, selected video quality or radio station/codec, Wi-Fi conditions, and whether a reboot helps. Attach screenshots and a **redacted** `MustardTube/data/launch.log` or `data/timing.log` if available. Remove any private URLs, credentials, search terms and tokens before posting.

## Disclaimer

**UNOFFICIAL, EXPERIMENTAL SOFTWARE — USE AT YOUR OWN RISK.** MustardTube is provided **AS IS**, without warranties to the fullest extent permitted by applicable law. Software or streams may crash, fail, consume storage/battery, or cause data loss. Users are responsible for complying with applicable laws, copyrights and website/platform terms. To the extent permitted by law, PapaZolaRis and contributors disclaim liability for losses, damage or legal consequences. Nothing purports to exclude non-excludable rights or liabilities. Not affiliated with YouTube, Google, Anbernic or MustardOS. MustardTube sources are MIT licensed; third-party components/assets retain their licences, including attribution for the bundled country flags.
