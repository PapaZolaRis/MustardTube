# MustardTube v0.9.11 Combined Beta 2 — RG35XX Plus / SP Test Guide

**Developer:** PapaZolaRis  
**Target:** Anbernic RG35XX Plus / SP on stock muOS 2606.0 Andromeda  
**Release status:** ARM64 development beta; physical handheld validation required before forum publication.

## What's new in Beta 2

- Radio countries sorted **A–Z by country name**, not by station count.
- Radio station lists and favourites sorted **A–Z by station name**.
- Radio lists no longer cover the section heading. Seven rows, a visible position counter and a slim scrollbar appear below the heading.
- **Hold UP/DOWN** on the D-pad to scroll continuously. Controller hold repeats after a short pause. **LEFT/RIGHT** jumps seven rows at a time.
- The About tab now includes a scrollable **user manual, troubleshooting advice, privacy notes, full disclaimer and licences**.
- About supports **UP/DOWN to scroll**, **LEFT/RIGHT to page**, **X for the next manual section**, **Y for the previous section**, and **B to return Home**.
- Home categories, Search video thumbnails, video resolution choices, Wasabi and radio favourites are preserved.

## Installation / update

1. Back up the existing `MustardTube/data` folder and keep the prior v0.9.11 Beta 1 installer for rollback.
2. Copy `MustardTube-v0.9.11-Combined-Beta2-RadioUX-Andromeda-ARM64.muxapp` to your SD card's `ARCHIVE` folder.
3. On the device, open **Applications → Archive Manager**, select the `.muxapp`, and install.
4. Connect to Wi-Fi, launch **MustardTube**, and open **Radio** with LB/RB.
5. Keep the old installer until Beta 2 has been tested on your device.

## Controller guide

| Screen | Button | Action |
|---|---|---|
| Anywhere | LB/RB | Previous / next top-level tab |
| Radio → Countries | A | Open selected country |
| Radio → Countries | X | Refresh country directory |
| Radio → Countries | Y | View favourite stations |
| Radio → Stations | A | Play selected radio station in Wasabi |
| Radio → Stations | X | Load the next 20 station entries (up to 80) |
| Radio → Stations / Favourites | Y | Add / remove favourite |
| Radio → Stations / Favourites | B | Return to country list |
| Radio lists | UP/DOWN | Select previous / next item |
| Radio lists | **Hold UP/DOWN** | Repeated scrolling without repeated presses |
| Radio lists | **LEFT/RIGHT** | Jump seven rows backward / forward |
| About | UP/DOWN, hold allowed | Scroll the manual |
| About | LEFT/RIGHT | Jump one manual page |
| About | X/Y | Next / previous manual section |
| About | B | Return Home |
| Home | LEFT/RIGHT | Change discovery category |
| Home | X | Refresh selected category |
| Search results | X | Load more search results, up to 40 |
| Video details | Y | Cycle quality (360p / 480p / experimental 720p) |
| Settings | X | Clear disposable cache; preserves radio favourites |
| Settings | Y | Clear local viewing history |
| Wasabi | MENU + START | Return to MustardTube (device setup permitting) |

## Device test checklist

- [ ] **Radio countries**: Australia, France, Germany, Japan, Malaysia, United Kingdom, United States, etc., appear alphabetically, not by station count.
- [ ] **Heading**: “COUNTRIES A–Z” and “STATIONS A–Z” remain readable, with no overlap.
- [ ] **Hold UP/DOWN**: selected row advances repeatedly on the device after a short delay and stops when released.
- [ ] **LEFT/RIGHT**: jumps seven rows; never leaves the bounds of the list.
- [ ] **Station list**: station names are alphabetical even after **X** loads more.
- [ ] **Favourites**: save two stations, see alphabetical favourites, and verify they survive Clear Cache.
- [ ] **Radio playback**: select a compatible station, listen in Wasabi, then return successfully.
- [ ] **About**: scroll from WELCOME to LICENSING and THANK YOU, page with LEFT/RIGHT, jump sections with X/Y, then return with B.
- [ ] **Home**: all six categories still display actual videos.
- [ ] **Search**: actual result thumbnail cards, repeated X load-more, then play 360p/480p.
- [ ] **Settings**: themes and cache clearing still work.

## Known limitations

- Country/station data comes from third-party services. Some station URLs will be unavailable, and some logos are missing.
- Network requests use small pages; up to 80 radio stations per selected country are cached in this beta. Listings are sorted alphabetically **within the loaded set**; the network backend requests results ordered by name.
- 720p recorded playback is experimental and does not support seeking. Live streams are not seekable.
- Offline video downloading is **not** included.
- Auto Performance governor configuration is **not** included. Users may set a per-app governor via muOS Application Options.
- This specific Beta 2 executable is cross-compiled and needs your physical-device check before a forum release.

## Debugging / feedback

If a problem occurs, record: exact device model, firmware, tab, selected station/video, the button sequence, whether Wi-Fi is connected, and whether the issue is reproducible. If possible, attach the app's `data/launch.log` with credentials, private paths and signed URLs removed. Use **L2 + R2 + X** on muOS (if supported by your configuration) for screenshots.

## Disclaimer

Unofficial community software. Provided **AS IS**, without warranties, to the fullest extent permitted by applicable law. Use at your own risk. It may crash, corrupt data or affect your device. Users are responsible for complying with laws, copyright and platform terms. To the fullest extent permitted by applicable law, PapaZolaRis and contributors disclaim liability for resulting losses, damage or legal consequences; non-excludable rights remain unaffected. MustardTube is unaffiliated with YouTube, Google, Anbernic and MustardOS. The historical Beta 2 build was MIT licensed; bundled third-party materials retain their own licences.
