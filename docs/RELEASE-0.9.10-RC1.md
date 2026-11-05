# MustardTube v0.9.10 RC1 — Home / Explore Videos

**Base:** the user-tested v0.9.9 PapaZolaRis About / Legal .muxapp. Do not use
an earlier experimental splash-based installer as the package baseline.

## Visual updates

* Unified the visible brand to **MustardTube** (capital M and T).
* Home now presents **up to four real YouTube search-derived videos** with
  thumbnails, titles, channels, and durations, rather than shortcut tiles.
* Labels them **Explore videos / YouTube search picks**, not official Trending.
* Up/Down selects; **A** opens the established details page; **X** refreshes;
  LB/RB still changes tabs. B behaviour is unchanged from v0.9.9.
* Cards load in a separate process so the rest of the UI remains responsive.
* The initial data is cached for one hour. Previous cards remain available if
  the internet goes down. Thumbnail cache is bounded by the existing backend.
* Home-origin selections return to Home after leaving the player.

## Non-changes

* Same muOS launcher, themes, icons, legal notice, PapaZolaRis credit, and
  Wasabi invocation code as the working About build.
* The existing search result format and search pagination are preserved.
* The existing 360p/480p seeking and 720p recorded experimental playback paths
  are untouched. Live streams have no seeking claim in the About tab.
* No PortMaster, streaming proxy, new runtime or manual dependency installation.

## Discovery semantics

The current discovery query is `ytsearch12:popular videos today` via the
already bundled yt-dlp, with metadata for at most four videos displayed. It
is **not** the official YouTube Trending service, and results may vary or be
unavailable due to network restrictions. `MUSTARDTUBE_HOME_QUERY` overrides
this search text for developers. No sponsored content or recommendation ranking
is claimed.

## Verification before hardware

* Existing backend regression suite plus Home success / retry / preservation
  cases, network-free: PASS.
* Home C regression tests: valid/invalid IDs, four-item selection, cache,
  selection to player and return to Home: PASS (native executable).
* ARM64 Clang cross-compilation: PASS; dynamic import set compatible with
  the running v0.9.9 PapaZolaRis About build. Standard Ubuntu aarch64 GCC
  remains the preferred reproducible compiler on the prepared VM.
* All original installer entries except `bin/mustardtube` and
  `scripts/youtube-backend.sh` should be verified byte-for-byte unchanged.
* Real RG35XX Plus / SP hardware test: **NOT YET COMPLETED**.

## Recommended handheld acceptance

1. Open Home. It should draw immediately, and then show up to four videos.
2. Check that titles and thumbnails are real, and X refresh works.
3. Select a Home card with A and verify details and normal 480p playback.
4. Exit Wasabi and return to Home; verify cards remain visible.
5. Search for a different term, return to Home, verify Search results are not replaced.
6. Turn Wi-Fi off and reopen. Previous Home cards should remain visible.
7. Verify About tab, theme selection, and recorded 720p warning still work.

Restore the known-working About / Legal v0.9.9 installer if the candidate fails.
