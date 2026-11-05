# v0.9.3 hardware candidate

The RG35XX Plus reported `yt-dlp --version` timeouts after 15 seconds even with no YouTube request. It also failed to resolve videos before 20–45 second external timeouts. We cannot yet establish why the executable stalls, but the launch delay is independent of the YouTube network and QuickJS challenge solving.

Candidate fix: replace single-file PyInstaller ARM64 yt-dlp 2026.10.26 with the official **unpackaged** ARM64 ZIP. Preserve all sibling dependencies when packaging. Close the UI/back-end mismatch (42s UI vs 36s backend) without increasing the actual extractor timeout.

No firmware change. Existing SD storage may still be a bottleneck. Validate the new `--version` startup time, direct search, direct video resolution, Wasabi playback, and repeat after sleep/reboot before declaring success. The exact runtime startup time is unmeasured in this authoring environment.
