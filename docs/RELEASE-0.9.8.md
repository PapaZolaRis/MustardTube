# MustardTube v0.9.8 — live HD trial

## Scope and non-goals

- Add third Y-selectable quality, HD 720p **for live HLS streams only**.
- Do not replace the successful v0.9.7 Node + unpackaged yt-dlp playback path.
- Do not promise improved visible resolution: RG35XX Plus / SP panels are
  640 × 480; typical 16:9 material displays at about 640 × 360.
- Do not download or permanently cache a live stream.

## Selection

- `quality.txt`: accepts 360, 480, 720; defaults to 360 for missing/invalid values.
- Y cycles 360 -> 480 -> 720 -> 360 on the Details page.
- At 720, normal VOD selection is intentionally still a combined A/V stream
  using the 480p-preferred format selection and its existing compatibility
  fallbacks; this is indicated in the details UI and status file.
- At 720, fallback only accepts an **exact 720p H.264 HLS video track** and
  `bestaudio[protocol^=m3u8]`. This prevents lying about 720p resolution.
- When 720p is not available, a descriptive error invites return to 360p.
- 720p HLS is joined with its audio rendition using `#EXT-X-MEDIA` and
  `#EXT-X-STREAM-INF` in a private 0600 master playlist. HLS cache remains
  short lived and is removed after Wasabi returns.

## Regression tests

- Existing search, Load More, timeouts, caching, QuickJS fallback and HLS
  360p/480p mock tests remain.
- New tests assert selected 720p format filters, actual 720p playlist metadata,
  caching by quality, unsupported HD failure and recorded-video 480p mode.
- UI regression tests assert three-state Y cycle and cache invalidation.
- UI SDL build, real 720p decoding performance, thermal behaviour and audio
  synchronization still require Ubuntu compilation and physical hardware checks.
