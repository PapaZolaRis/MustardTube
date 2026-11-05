# MustardTube v0.9.4 — CPU governor and reliability candidate

## What hardware tests established
- RG35XX Plus / muOS Andromeda: original yt-dlp `--version` starts in 17 seconds under powersave (288 MHz) versus 4 seconds under performance (1.8 GHz).
- Unpackaged runtime: 11 seconds versus 3 seconds, but occupies ~148 MB compared to ~39 MB standalone.
- A real public YouTube stream URL successfully resolved in 41 seconds at performance governor.
- v0.9.1's 36-second backend watchdog could never complete that 41-second request.

## Changes
1. Restore the smaller verified, standalone ARM64 `yt-dlp` runtime; preserve bundled QuickJS and licences.
2. Set backend resolver watchdog to 60 seconds; align native UI playback wait to 66 seconds. The UI has a 6-second margin for backend completion and file handoff.
3. Preserve v0.9.3 truthful two-action Home, working 40-result Load More, caches, 360p/480p preferences, glyph, and Wasabi.
4. **Do not manipulate sysfs CPU settings from the app.** muOS Andromeda offers Applications > Application Options > MustardTube > Governor. Select Performance for performance qualification; system manages app launch and restore.

## Limits
- 41-second preparation remains too slow and requires follow-on optimisation. This candidate prevents prematurely rejecting streams that would otherwise succeed; it does not guarantee fast playback.
- A successful extractor URL does not yet prove Wasabi can decode/play the specific stream. Verify audio, video, aspect ratio, and `MENU + START` return on device.
- Network extraction and CPU governor are still dependent on device environment; do not claim reliability until hardware trials pass.
