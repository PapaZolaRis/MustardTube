# MustardTube v0.9.7 — YouTube live broadcasts (hardware test candidate)

## Evidence from the RG35XX Plus / muOS Andromeda

The user verified the specific active broadcast `SFxxr-rNyNQ` exposes 640×360 and
854×480 video-only HLS formats and separate HLS audio. The working **MT Live Probe**
launched through muOS Applications successfully played a local A/V control, live
HLS audio-only, and a synthesized combined HLS playlist **with moving picture
and audible sound** in Wasabi. An SSH-launched Wasabi test failed audio setup;
therefore actual testing must run from the Applications menu.

## v0.9.7 implementation

- Retains proven v0.9.6 Node-first and unpackaged ARM64 yt-dlp runtime, normal
  video formats, UI, 40-result X Load More, thumbnails, theme and app logo.
- Extends ONE `yt-dlp --get-url` format selection: first try combined normal
  video/audio; if unavailable, try **HLS video-only plus HLS audio-only** formats.
- Only accepts two valid HTTPS URL lines for the separate-track HLS case;
  produces a private (`0700` parent, `0600` file), temporary local HLS master
  playlist that Wasabi opens in `--live` mode with normal muOS app context.
- Video resolution preference 640×360 or 854×480; playlist retains native
  widescreen aspect ratio. Audio uses the available HLS audio rendition.
- A/V playlist is re-used through the details/launcher handoff without
  re-extracting, but has a **45-second maximum readiness TTL** and is not placed
  in v0.9.6's five-minute VOD cache. The launcher deletes it after Wasabi returns.
  The backend also cleans expired/invalidated live state.
- Does not log stream URLs, query strings, signed manifests, video IDs or
  credentials. The backend tests use mock signed URLs and do not access YouTube.
- `yt-dlp` extraction can still fail for restricted, ended, upcoming, or
  incompatible live events. Video+audio availability may differ across channels.

## Hardware acceptance test

1. Keep a backup of the stable v0.9.6 installation. Set MustardTube's muOS
   per-application governor to Performance; do not change system-wide settings.
2. Test the same live channel `SFxxr-rNyNQ` at Fast 360p: moving picture AND
   synchronized sound, ongoing playback, exit `MENU + START`, return to details.
3. Change Y to Clear 480p and repeat. Verify picture, audio, aspect ratio and
   continuous playback. Short live URL TTL means re-resolving after long idle.
4. Play several normal recorded videos after live playback; compare to
   v0.9.6's roughly 10–13 second video preparation baseline.
5. Search, X Load More, switch videos, return from Wasabi, clear cache, and
   confirm `/tmp` remains ~2% and `_MEI` directory count remains zero.
6. The `data/timing.log` should show `stream=live` or `stream=vod` without URLs.
   Record errors/elapsed seconds only. Revert to v0.9.6 if anything regresses.

## Known limitations

- A live HLS pair is a format compatibility path, not a guarantee all YouTube
  lives can be played. If a live source has no accessible audio or supported
  video codec, the app should fail safely. Zero separate audio references in a
  source playlist does not prove no audio is available; synthesis was tested.
- Only Node/QuickJS and bundled yt-dlp are used. No PortMaster or separate apps.
- Source candidate has not been compiled or validated on the user's handheld.
