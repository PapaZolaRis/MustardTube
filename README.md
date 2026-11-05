# MustardTube — YouTube & Internet Radio for muOS

**v0.9.11 Beta 2.1 — Multi-Display Compatibility Candidate** · **Developed by PapaZolaRis** · Community testing build

MustardTube is an unofficial, lightweight, controller-driven YouTube video browser
and Internet Radio client for devices running
**muOS 2606.0 Andromeda**. It launches video and radio streams using the
**stock Wasabi player**—no PortMaster, Google login, YouTube API key or manual
runtime dependency installation is required for users of the prebuilt package.

> **Public beta:** Beta 2.1 has been tested successfully on an **Anbernic RG35XX Plus
> (640 × 480)** running muOS 2606.0 Andromeda. Other supported devices, resolutions,
> controller layouts and hardware families still need community testing. **Beta 2 is
> retained as a rollback option**, not a separate release required for 640 × 480 screens.

## Which version should I install?

| Release | What it provides | When to use it |
| --- | --- | --- |
| **[v0.9.11 Beta 2.1](https://github.com/PapaZolaRis/MustardTube/releases/tag/v0.9.11-beta.2.1)** | All Beta 2 features, plus automatic display-size detection, aspect-preserving scaling, corrected pointer positions and safe screenshot capture at native resolution. | **Recommended beta for testing, including 640 × 480 devices.** Tested successfully on the RG35XX Plus; other hardware is not yet validated. |
| **[v0.9.11 Beta 2](https://github.com/PapaZolaRis/MustardTube/releases/tag/v0.9.11-beta.2)** | The original 640 × 480 interface with YouTube Home categories, Search thumbnails, country-based Internet Radio, favourites, scrolling and a built-in guide. | **Fallback** if Beta 2.1 introduces a regression on a device where Beta 2 was working. Its fixed-size interface may not fit different aspect ratios correctly. |

**Important:** The RG35XX Plus has a **640 × 480** screen (not 940 × 480). Since Beta 2.1 has been tested on this device and preserves its original layout, users do **not** need to downgrade just because their screen is 640 × 480. When reporting a regression, please include the device model and whether Beta 2 still works.

**Beta 2 history:** Beta 2 introduced the polished Radio country/station interface with alphabetical sorting, held-D-pad navigation and a scrollable About/User Guide. Beta 2.1 is an **incremental display-compatibility update**, not a replacement or redesign of those core features. The app remains an unofficial community beta, and no release is guaranteed to work across every muOS Andromeda-supported device.

## Features shared by Beta 2 and Beta 2.1

- **Home:** Explore, Popular, For You, Music, Gaming and Tech discovery categories;
  real thumbnails, video titles and channels. These are *search-based* discovery
  results, not official YouTube Trending rankings. 'For You' uses an optional
  local history approach, not a YouTube-account recommendation feed.
- **Search:** Real video thumbnail cards and repeated **X** to load more results,
  up to 40.
- **Playback:** 360p default, selectable 480p, and experimental 720p for
  supported streams through Wasabi. Seeking works on compatible recorded
  360p/480p streams; **recorded 720p and live streams are not seekable**.
- **Internet Radio:** Country lists and station names sorted alphabetically,
  249 bundled country flags, optional small station logos, favourites, station
  directory paging and stream playback through Wasabi.
- **Navigation:** Hold D-pad Up/Down for radio list scrolling, Left/Right to
  jump seven entries, position indication and scroll markers.
- **Settings:** Midnight, Forest and Paper themes, Clear Cache and Clear
  Viewing History. Cache maintenance keeps radio favourites.
- **About / User Guide:** Scrollable usage guide, troubleshooting, attribution,
  legal notice and controls.

## Additional screen adaptation in Beta 2.1

- Automatically detects the screen size using SDL2; on **640 × 480** devices the
  working Beta 2 layout is kept unchanged.
- On 3:2, 1:1, 16:9 and larger 4:3 screens the UI is **centered and scaled
  without stretching or clipping**. There may be unused side/top margins.
- Handles pointer selection relative to that viewport, and safely captures
  native-resolution screenshots.
- Keeps the same YouTube, Radio and Wasabi processing; **no extra decoder or
  background processes** are added.

See [Multi-display compatibility testing](docs/DISPLAY-COMPATIBILITY-BETA2.1.md).

## Compatibility

| Requirement | Status |
| --- | --- |
| RG35XX Plus | **Beta 2.1 tested successfully** by the project maintainer on 640 × 480 hardware |
| RG35XX SP | Primary target; Beta 2.1 needs separate confirmation on this device |
| muOS 2606.0 Andromeda | Target firmware |
| Stock Wasabi media player | Required, provided by target firmware |
| Network connectivity | Required for video and radio; some muOS devices require an external Wi-Fi adapter |
| PortMaster | Not required |
| Other Andromeda-supported devices | Display adaptation provided; **device validation still required** |
| Older muOS versions | Not targeted |

## Download and installation

**End users should install the prebuilt `.muxapp` from [GitHub Releases](https://github.com/PapaZolaRis/MustardTube/releases)**
(**not** a ZIP of this source repository). For the latest multi-display test, open
`v0.9.11-beta.2.1`. If you encounter a new problem, report it and try the separate
`v0.9.11-beta.2` installer as a fallback after backing up your application data.

1. Download `MustardTube-v0.9.11-Beta2.1-MultiDisplay-Andromeda-ARM64-TEST.muxapp`.
2. Copy the `.muxapp` to the SD card's `ARCHIVE` folder.
3. On the handheld, use **Applications → Archive Manager** to install it.
4. Connect Wi-Fi and open MustardTube from **Applications**.
5. If updating from an earlier build, back up its `data` directory first.

For device-specific testing see the [Beta 2.1 display-compatibility guide](docs/DISPLAY-COMPATIBILITY-BETA2.1.md). For the original controls and Beta 2 behaviour, see the [Beta 2 guide](docs/TEST-GUIDE-BETA2.md).

### If you need to roll back to Beta 2

1. Back up the existing MustardTube `data` folder, particularly your radio favourites and preferences.
2. Download the **Beta 2** `.muxapp` from its own GitHub prerelease (linked above).
3. Install via muOS **Applications → Archive Manager**, following the same installation process.
4. Check whether the issue also occurs in Beta 2, and include the comparison in a [GitHub issue](https://github.com/PapaZolaRis/MustardTube/issues).

Rollback is a troubleshooting option, not a guarantee that downgrading automatically resolves every problem.

## Core controls

| Control | Action |
| --- | --- |
| LB / RB | Switch main tabs |
| D-pad | Navigate |
| A / B | Select / back |
| X in Home | Refresh discovery cards |
| X in Search | Load more results |
| Y in video details | Change video quality |
| Radio: Hold Up/Down | Repeat scrolling |
| Radio: Left/Right | Jump seven entries |
| Radio: X | Refresh / load more |
| Radio: Y | Add or remove favourite |
| Settings: X | Clear temporary cache |
| Settings: Y | Clear viewing history |
| About: Up/Down | Scroll guide |
| About: Left/Right | Page through guide |
| About: X/Y | Jump between sections |
| In Wasabi: MENU + START | Return to MustardTube (tested on target configuration) |

## Feedback and issue reports

Please use [GitHub Issues](https://github.com/PapaZolaRis/MustardTube/issues) to report problems. Include:

- Handheld model, muOS firmware version, and MustardTube version (Beta 2 or Beta 2.1).
- Whether the same problem occurs in the other beta, if tested.
- Which tab or station/video was involved, and how to reproduce it.
- Expected and actual behaviour, plus any screenshot.
- A **redacted** `MustardTube/data/launch.log` if available.

**Never publish signed media URLs, passwords, API credentials, personal search
history or other sensitive information** in issues or screenshots.

## Source tree

- `src/` — native SDL2 application, UI and controller handling.
- `scripts/youtube-backend.sh` — video discovery and playback metadata.
- `scripts/radio-backend.js` — internet radio directory, filtering, caching.
- `packaging/` — muOS launch configuration.
- `assets/` — graphics, flag atlas and themes.
- `tests/`, `scripts/test-*` — regression tests.
- `docs/` — release history, build instructions and notices.

Run `sh scripts/test-display-compat.sh` for pure screen-fit geometry tests.

The repository contains **source and small assets only**, not bundled yt-dlp,
Node, Wasabi or player binaries. The prebuilt device installer is published as a
GitHub Release asset.

## Developer builds

See [Build and packaging notes](docs/BUILDING.md) if present in your repository, as well as the supplied build scripts. Cross-compilation requires a
working AArch64 development environment and SDL2/SDL2_ttf/SDL2_image development
libraries. **End users installing the `.muxapp` do not need these build tools.**

## Legal and third-party notices

This is **unofficial software**, provided **AS IS**, without warranty, to the
fullest extent permitted by law. Use at your own risk and obey copyright,
platform terms and applicable law. Not endorsed by YouTube, Google, Anbernic
or MustardOS. Read [`LICENSE`](LICENSE) (MIT) and
[`docs/PAPAZOLARIS-USER-NOTICE.txt`](docs/PAPAZOLARIS-USER-NOTICE.txt).

Country flag atlas uses Twemoji artwork under CC BY 4.0; see
[`assets/flags/ATTRIBUTION.txt`](assets/flags/ATTRIBUTION.txt).
Bundled components in the device installer have their own third-party licences.

## Release and source-version notes

Each release is distributed with its matching `.muxapp` asset. For source snapshots, use the version tag rather than assuming that the current `main` branch always matches an older release. If the GitHub prerelease tag was created *before* its updated source was committed, verify that its tagged source contains the correct version; later commits to `main` do not update an existing tag.

**Developer:** PapaZolaRis
