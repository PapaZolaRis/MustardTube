# MustardTube v0.9.11 Beta 2.4.1 — Safe OTA preparation

Target: stock muOS 2606.0 Andromeda, RG35XX Plus and RG35XX SP.

## Changes

- Retains the approved four themes, header, uniform video cards, radio and video behaviour.
- Adds a once-per-day *optional* release check at app launch, using the existing
  bundled Node.js process. No daemon, polling loop, or automatic download.
- Update selection is pinned to a specific GitHub release version, filename,
  size, checksum and download URL. Changes require a new Check for updates.
- `Prepare verified update` downloads the selected `.muxapp` and checks SHA-256,
  ZIP CRC and archive entry layout, then atomically places it in SD1 `ARCHIVE`.
- Creates a separate timestamped, integrity-checked recovery snapshot of user
  preferences, favourites, viewing history, volume and theme selection, without
  copying signed URLs, cache, radio playback state or logs.
- `Exit for Archive Manager` exits cleanly. The user still selects the package
  in muOS Applications > Archive Manager. MustardTube never overwrites itself.
- Every new option fits the existing 640x480 layout; no new libraries.

## Important limitations

This is a **safe staged OTA**, not fully autonomous installation. The stock
Archive Manager's private install implementation is not an endorsed public API
for a third-party application to invoke with a package path. An unverified
self-overwrite would risk corrupting the user's working application.

Successful real-world download, Archive Manager update installation, SD1/SD2
storage cases, data persistence, and manual recovery remain unverified until
hardware tests with a newer official test release. A GitHub-provided checksum
is an integrity check, not an independent publisher signature.

Keep Beta 2.4.0's `.muxapp` installer and do not publish until source and
third-party licensing requirements have been audited.
