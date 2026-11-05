# Release candidate 0.9.6 — Node.js video preparation

Observed on RG35XX Plus, muOS Andromeda, Performance governor:
- QuickJS: 41 seconds to resolve known public video
- Node.js 24.22.2: 13 seconds to resolve same video
- `yt-dlp-ejs` 0.8.0 solver; no signed URLs exposed in diagnostics
- `/tmp` stable at 2%, zero `_MEI` folders with v0.9.5 onedir extractor

Changes: official SHA256-verified Node.js ARM64 packaged as default JS runtime; QuickJS fallback; safe solver timing labels; no video-format, UI, launcher or search pagination changes.

Not yet verified: built ARM64 application installed on handheld, all search/video types, RAM pressure, battery impact. Preserve v0.9.5 backup.
