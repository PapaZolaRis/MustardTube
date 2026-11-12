# MustardTube — YouTube and Internet Radio for muOS

**Current source version: v0.9.11 Beta 2.4.2 (GPL-3.0-only; OTA progress and network-error refinement).**
**Beta 2.4.2 hardware report: OTA upgrade and functional checks passed on RG35XX Plus; SP testing not yet confirmed for this build.**
**Previously validated baseline: Beta 2.4.0 on RG35XX Plus and RG35XX SP.**
**Historical MIT baseline: Beta 2.2.7; earlier permissions are unchanged.**

![MustardTube branding](assets/branding/mustardtube-wordmark-reference.png)

**Developer: PapaZolaRis**
**Target firmware: muOS 2606.0 Andromeda (ARM64)**

MustardTube is an unofficial, lightweight, controller-friendly YouTube browser and Internet Radio application for muOS handhelds. It provides video discovery, search and playback, a radio station browser, and a dedicated audio-only radio player that can continue playing while you play games.

Video playback uses the stock **Wasabi** media player. Background radio uses a bundled, dedicated **`ffplay-radio`** player and its controller; it does **not** run Wasabi in the background. The prebuilt `.muxapp` is designed for installation without PortMaster, a Google account, a YouTube API key or manual installation of additional system dependencies.

**[Official releases](https://github.com/PapaZolaRis/MustardTube/releases)** | **[Report an issue](https://github.com/PapaZolaRis/MustardTube/issues)**

## Release status and compatibility

Beta 2.4.2 is a **pre-release**, not a stable release. Testing status for this precise build differs by device:

| Device | Firmware | Hardware test status |
| --- | --- | --- |
| Anbernic RG35XX Plus | muOS 2606.0 Andromeda | Beta 2.4.1 → Beta 2.4.2 OTA and functional checks passed (user-reported) |
| Anbernic RG35XX SP | muOS 2606.0 Andromeda | Beta 2.4.0 validated; Beta 2.4.2 OTA test still pending |

Background radio, PPSSPP game-audio mixing, in-game volume shortcuts, automatic game-volume reset, and interface behavior were confirmed during these tests. Compatibility with every emulator or other muOS-supported handheld has **not** been established.

MustardTube retains the multi-display adaptation introduced in Beta 2.1. The original 640×480 interface is scaled and centered on other supported aspect ratios, including 3:2, 1:1 and 16:9. Unused margins may appear; this is intentional. Scaling has been tested in development, but other display/device combinations still need hardware validation.

Older muOS versions, including Goose and Jacaranda, are not current development targets.

## Features

### YouTube Home and Search

- Discover videos through **Explore, Popular, For You, Music, Gaming and Tech** categories.
- Browse four Home thumbnail cards at a time. Press **X** to cycle through additional real discovery results, including more than 100 distinct results where YouTube supplies them.
- Search YouTube from the handheld and press **X** repeatedly to load additional results, up to **120** in total.
- View video details before starting playback.
- Home discovery is search-based, not an official YouTube Trending feed. **For You** uses optional local viewing history rather than an account-based YouTube recommendation feed.

### Video playback

- Uses the stock Wasabi player supplied by muOS Andromeda.
- **360p** is the lightweight default; **480p** can be selected where available.
- **720p** playback is experimental and works only with compatible streams.
- Seeking may work on compatible recorded 360p/480p videos. Recorded 720p and live streams are not seekable in the supported workflow.
- Some videos or stream formats may be unavailable or incompatible with Wasabi.

### Internet Radio

- Browse stations by country; country and station lists are alphabetical.
- Includes **249** bundled country flags, available station logos, favorites and paginated station results without the previous 80-station cap. The number available depends on the directory or playlist, safe network limits and device memory.
- Uses a built-in audio-only radio player for normal radio listening and background playback.
- Can continue playing Internet Radio while the user leaves MustardTube and plays games.
- The **Radio / Now Playing** screen displays station details, radio status and separate volume controls. The redundant Settings radio-volume slider has been removed; Settings still controls the in-game hotkey mode.
- A Wasabi radio playback option is also available as a fallback for compatible stations.
- Station availability depends on the broadcaster, network and stream format.

### Independent Radio and Game Volume

Beta 2.2.7 provides separate **Radio Volume** and **Game Volume** sliders in **Radio / Now Playing**. During an active background radio session, the game slider controls a detected, supported game-audio stream while **RADIO+GAME** mode is enabled.

- Radio and game levels can be adjusted independently in 5% steps.
- In-game volume shortcuts support single-press adjustment and faster press-and-hold repeat.
- The saved Game Volume preference synchronizes with shortcut adjustments.
- When background radio stops, MustardTube attempts to restore the **managed game-audio stream to 100%**, including identity-checked recovery in supported game-exit/relaunch cases.
- Recovery is bounded and best-effort; it is not guaranteed for every game, emulator or delay between exit and relaunch.
- The 100% reset **does not change the muOS master volume or unrelated applications**.

#### In-game shortcuts

Enable **RADIO+GAME** to control both sources during gameplay.

| Buttons | Action |
| --- | --- |
| **L2 + R2 + Left** | Radio volume down |
| **L2 + R2 + Right** | Radio volume up |
| **L2 + R2 + Up** | Game volume up |
| **L2 + R2 + Down** | Game volume down |

Each tap changes volume by 5%. Holding the combination repeats the adjustment.

Hotkey modes are **OFF**, **RADIO** (radio-only shortcuts), and **RADIO+GAME** (radio and game shortcuts). The mode can be cycled with **X** on **Radio / Now Playing** or **START** on the **Settings** screen.

Some games or emulators may map these same buttons to their own controls. Test the mapping with your preferred emulator.

### GitHub Releases updates (Beta 2.4.2)

From **Settings**, press **SELECT** for **Software updates**. The app can check
for new releases **once per day when launched** (default ON, can be turned OFF).
No automatic download occurs. You may also press **Check for updates** manually.

When a newer release is offered, choose **Prepare verified update**. The updater
pins the release version, asset filename, digest and size; rechecks these
before downloading; and verifies SHA-256, ZIP CRC and archive layout. A
bounded snapshot of theme preferences, favourites, volume and viewing history
is saved to `ARCHIVE/.mustardtube-recovery/` for supervised recovery. The
installer itself is placed in SD-card `ARCHIVE`. Beta 2.4.2 adds a byte-based download progress display, verification stages, and user-friendly network-error messages.

When Ready, choose **Exit for Archive Manager**, then select the `.muxapp` in
**Applications → Archive Manager**. MustardTube deliberately does not invoke
undocumented muOS installation internals or overwrite itself. The update is
user-controlled, and automatic rollback is not yet available. Keep a known-good
installer for rollback. Only official public GitHub Releases with correctly
named assets and GitHub-provided SHA-256 digests are eligible. GitHub digests
provide integrity, **not independent maintainer signing**.

For publishing requirements and recovery information, see
[OTA release publishing](docs/OTA-RELEASE-PUBLISHING.md).

### Themes and maintenance

- Four themes: **Midnight (Black Gold)**, **Paper (Champagne Linen)**, **Forest (Botanical Club)**, and **Acid Bloom**.
- **Clear Cache** removes disposable cached content while preserving user preferences and radio favorites.
- **Clear Viewing History** is available separately.
- Includes an in-app About / User Guide with controls, troubleshooting and legal notices.

## Download and installation

**Previously validated baseline:** `MustardTube-v0.9.11-Beta2.2.7-HD-Streaming.muxapp`

1. Visit the [official GitHub Releases page](https://github.com/PapaZolaRis/MustardTube/releases).
2. Download a verified installer for the version you intend to install, ensuring that the installer and corresponding source belong to the same release.
3. Copy the `.muxapp` to the SD card's **`ARCHIVE`** folder.
4. On the handheld, open **Applications → Archive Manager** and install the package.
5. Connect to Wi-Fi and launch **MustardTube** from Applications.
6. For in-game dual volume control, choose **RADIO+GAME** in the radio player or Settings.

**Before updating:** Keep a known-good installer. The OTA Prepare operation
backs up selected preferences, history and favourites, but a manual backup is
still prudent before using Archive Manager.

The `.muxapp` is a compiled end-user installer. The separately available **version-matched source ZIP** contains source code and small project assets, **not** the compiled installable application.

## Main controls

| Control | Action |
| --- | --- |
| **LB / RB** | Switch main tabs |
| **D-pad** | Navigate menus and lists |
| **A / B** | Select / back |
| **X on Home** | Refresh discovery cards |
| **X on Search results** | Load more results |
| **Y on video details** | Change playback quality |
| **Radio browser: Hold Up/Down** | Continuous list scrolling |
| **Radio browser: Left/Right** | Jump through the station or country list |
| **Radio browser: X** | Refresh countries or load more stations |
| **Radio browser: Y** | Favorites action |
| **Radio browser: A on station** | Open built-in radio playback |
| **Radio browser: START on station** | Start background radio |
| **Radio browser: SELECT** | Stop background radio |
| **Radio / Now Playing: Up/Down** | Select Radio Volume or Game Volume |
| **Radio / Now Playing: Left/Right** | Adjust selected volume by 5% |
| **Radio / Now Playing: X** | Cycle in-game hotkey mode |
| **Radio / Now Playing: START** | Exit to the games interface while radio continues |
| **Radio / Now Playing: SELECT** | Stop radio |
| **Radio / Now Playing: Y** | Open Wasabi radio fallback |
| **Radio / Now Playing: B** | Return to station list |
| **Settings: START** | Cycle hotkey mode |
| **Settings: SELECT** | Open software updates |
| **Software updates: A** | Check, download, or change update channel |
| **Software updates: B** | Return to Settings |
| **Settings: X / Y** | Clear Cache / Clear Viewing History |
| **About: Up/Down** | Scroll guide |
| **About: X / Y** | Jump between guide sections |
| **In Wasabi: MENU + START** | Return to MustardTube on tested devices |

The physical button names shown by muOS may differ slightly from the SDL/controller mappings above.

## Performance

MustardTube is designed to minimize runtime overhead during gaming. The background radio helper is lightweight, and the game-volume controls target identifiable PipeWire streams rather than modifying global audio settings.

For faster navigation and searches, the optional per-application **Performance** CPU governor may help. Set it through muOS **Applications → Application Options → Governor**. This can increase battery consumption and heat, and is not required for installation.

## Known limitations

- This is a pre-release tested primarily on the RG35XX Plus and RG35XX SP with muOS Andromeda.
- Hardware behavior may vary on other devices, display formats and emulators.
- The interface scales from a 640×480 layout rather than providing separate native layouts for all screens.
- Some video formats and radio streams may be unavailable or unsupported.
- Recorded 720p playback is experimental and non-seekable; live streams are non-seekable.
- A network connection is needed for online video and radio. Some handhelds require an external Wi-Fi adapter.
- In-game hotkeys may conflict with emulator button mappings.
- The game-audio reset is limited to identified, managed streams and supported recovery cases.
- Offline video downloads and automatic CPU governor configuration are not included.

## Feedback and bug reports

Please report issues at **[GitHub Issues](https://github.com/PapaZolaRis/MustardTube/issues)**. Include the device model, muOS version, MustardTube version, game/emulator if relevant, reproduction steps and expected versus actual results.

Screenshots and a **redacted** `MustardTube/data/launch.log` may help. **Do not publish** passwords, personal search history, signed media URLs, API credentials or other sensitive data.

## Source and developer builds

The GitHub source archive includes native SDL2 source, JavaScript and shell controllers, tests, packaging scripts, assets and licensing notices. It deliberately omits compiled runtime dependencies, personal device data, generated installers and build output.

Relevant components include:

- `src/main.c` — SDL2 interface and input handling.
- `src/game_mixer.c`, `src/game_mixer.h` — game-stream volume control and recovery.
- `src/radio_hotkeys.c` — in-game shortcut handling.
- `scripts/background-radio.js` — background radio controller.
- `scripts/radio-backend.js` — station discovery and caching.
- `scripts/youtube-backend.sh` — YouTube discovery and playback preparation.
- `scripts/test-game-volume.sh`, `scripts/test-beta22-source.sh` — development regression tests.
- `assets/`, `packaging/`, `tests/`, `docs/` — assets, packaging, test fixtures and documentation.

Development builds require an appropriately configured AArch64 cross-compilation environment and the project build scripts. The current packaging workflow also uses a compatible baseline `.muxapp` to retain required runtime components. End users installing the release `.muxapp` do not need these developer tools.

## Legal and third-party notices

**UNOFFICIAL SOFTWARE — USE AT YOUR OWN RISK.** MustardTube is provided **AS IS**, without warranty, to the fullest extent permitted by applicable law. Users must comply with applicable law, copyright requirements and third-party platform terms.

MustardTube is not affiliated with or endorsed by YouTube, Google, Anbernic or MustardOS.

Beginning with the **first published Beta 2.2.8 release**, the first-party
MustardTube code that the relevant rightsholders may relicense is distributed
under [GNU GPL version 3 only](LICENSE) (`GPL-3.0-only`). Previously published
MIT-licensed releases remain available under their original MIT permissions;
this change does not retroactively remove those permissions. Forks, commercial
redistribution and modifications are welcome, subject to the applicable licenses.
**Official repository:** https://github.com/PapaZolaRis/MustardTube

The source tree and corresponding release source archive should be published
alongside GPL-covered executable releases. GPLv3 distribution requires access
to the **actual Corresponding Source for the exact binaries distributed**;
merely pointing to a different release or newer code is not sufficient.
Before distributing an installer, complete the rights and bundled dependency
checks in [RELICENSING-CHECKLIST.md](docs/RELICENSING-CHECKLIST.md) and
[third-party review](docs/THIRD-PARTY-LICENSE-REVIEW.md).

Third-party binaries and artwork have their own applicable licenses and notices. The bundled flag atlas uses Twemoji artwork under CC BY 4.0; see [`assets/flags/ATTRIBUTION.txt`](assets/flags/ATTRIBUTION.txt). Additional notices are available in [`docs/PAPAZOLARIS-USER-NOTICE.txt`](docs/PAPAZOLARIS-USER-NOTICE.txt).

**Developer:** PapaZolaRis

## Beta 2.4.2: Four-theme UI and verified OTA preparation

All primary screens use a compact 4:3 left navigation sidebar and the approved
MustardTube branding. Home and Search Results display **four equal-size video
cards** (477 × 54 logical pixels) with matching 86 × 48 thumbnails. There is
**no enlarged featured card**. Search keyboard, radio directory, Now Playing,
Recent, Settings, About, video details and the matching pointer areas have been
adapted to the same 640 × 480 frame. The app icon is decoded only once.

Controller navigation, search, Wasabi, recorded/live streaming, radio mixing,
preferences, existing four-image limits, release licensing and build dependencies
are unchanged.

**Performance budget:** at most one small, opt-out GitHub release check per
day during app launches; no background polling, animated backgrounds,
render targets, or concurrently decoded video thumbnails. The update screen
shows byte-based download progress without introducing a graphics dependency.
Beta 2.4.2 has been tested by the maintainer on RG35XX Plus; confirm this
exact build on RG35XX SP before describing it as supported on both devices.
