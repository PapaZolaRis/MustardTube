# MustardTube — YouTube & Internet Radio for muOS

**v0.9.11 Beta 2.1 — Multi-Display Compatibility Candidate** · **Developed by PapaZolaRis** · Community testing build

MustardTube is an unofficial, lightweight, controller-driven YouTube video browser
and Internet Radio client for devices running
**muOS 2606.0 Andromeda**. It launches video and radio streams using the
**stock Wasabi player**—no PortMaster, Google login, YouTube API key or manual
runtime dependency installation is required for users of the prebuilt package.

> **Beta status:** Native UI/back-end source tests and ARM64 packaging have been
> checked. This candidate has **not** been validated on all 18 Andromeda devices;
> compatibility with different SoCs, controller maps, runtimes and Wasabi must
> be verified on the device. **Beta 2 remains the fallback.**

## Features in Beta 2.1

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

## Screen adaptation in Beta 2.1

- Automatically detects the screen size using SDL2; on 640x480 devices the
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
| RG35XX Plus / RG35XX SP | Existing primary test targets |
| muOS 2606.0 Andromeda | Target firmware |
| Stock Wasabi media player | Required, provided by target firmware |
| Network connectivity | Required for video and radio; some muOS devices require an external Wi-Fi adapter |
| PortMaster | Not required |
| Other Andromeda-supported devices | Display adaptation provided; **device validation still required** |
| Older muOS versions | Not targeted |

## Download and installation

**End users should install the prebuilt `.muxapp` from [Releases](../../releases)**
(**not** this source ZIP). In GitHub, choose **Releases** on the repository page
and open the `v0.9.11-beta.2.1` prerelease.

1. Download `MustardTube-v0.9.11-Beta2.1.muxapp`.
2. Copy the `.muxapp` to the SD card's `ARCHIVE` folder.
3. On the handheld, use **Applications → Archive Manager** to install it.
4. Connect Wi-Fi and open MustardTube from **Applications**.
5. If updating from an earlier build, back up its `data` directory first.

For device-specific testing see [Beta 2.1 display-compatibility guide](docs/DISPLAY-COMPATIBILITY-BETA2.1.md). For controller details see [Beta 2 guide](docs/TEST-GUIDE-BETA2.md).

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

Please use [GitHub Issues](../../issues) to report problems. Include:

- Handheld model and muOS firmware version.
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

See [Build and packaging notes](docs/BUILDING.md). Cross-compilation requires a
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

**Developer:** PapaZolaRis
