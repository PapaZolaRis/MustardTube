# v0.9.9 — recorded-video HD validation candidate

## Evidence before development

Device A/B diagnostic on `aqz-KE-bpKQ` with v0.9.8 and Performance governor:
A 360p existing selection 14s, one URL; B tv-only client 5s but ERROR; C skip
configs/initial_data 13s, one URL; D muxed H.264 720p 13s but no format;
E HLS H.264 720p video + HLS audio 14s, two URLs. Therefore no demonstrated
under-five-second VOD shortcut, but a real 720p HLS recorded-video candidate.

## Implementation

- 720p first requests a *true* muxed H.264 720p format; if unavailable,
  selects video HLS 720p + audio HLS in the same yt-dlp extraction.
- `--print-to-file %(live_status)s` records state during the same request;
  metadata is not logged and introduces no extra YouTube invocation.
- Output stays private; `live` and `vod_hls` share a short-lived private
  master-playlist builder but use distinct Wasabi launch modes.
- Cache separates normal media slots from short-lived HLS. After any Wasabi
  session, live/VOD HLS is invalidated. No signed URLs in timing logs.
- The existing 360/480 formats, Node/QJS preference and search are unchanged.
- Known limitation: HLS VOD seeking and audio sync have not been confirmed on
  the device; do not describe as working until observed.

## Tests

Network-free integration verifies live HLS unaffected, recorded HLS classified
correctly, no false 480p HD, cache reuse and invalidation, quality status,
plus existing search/playback/timeout tests. Native SDL and actual hardware
playback must be validated on Ubuntu/device.
