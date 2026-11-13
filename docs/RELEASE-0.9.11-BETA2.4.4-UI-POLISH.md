# MustardTube v0.9.11 Beta 2.4.4 — UI polish (candidate)

Target: muOS 2606.0 Andromeda on RG35XX Plus / RG35XX SP.

## Changes since Beta 2.4.3

- Fix clipped/uneven border corners in Software Updates when using horizontal tabs by drawing these borders directly in final 640x480 pixel coordinates.
- Replace the large yellow video-details play disc with a smaller charcoal outlined play badge and directional play triangle. Creator thumbnail and app logo stay intact, and no extra graphic asset is required.
- Correct the updater's hard-coded local-version constant to Beta 2.4.4, to avoid detecting an already installed version as an update.
- No changes to search/playback extraction, Wasabi handoff, radio, game audio mixing, preferences, tab input behaviour, or bundled runtimes.

## Why the release number is 2.4.4

The updater installed on Beta 2.4.3 accepts `betaMAJOR.MINOR.PATCH`, not a suffix such as `2.4.3a`. Using the next numeric version permits ordinary GitHub OTA discovery. YouTube Shorts remains planned separately.

## Testing

Host-side source regressions and OTA logic must pass before an Ubuntu ARM64 build.
Real rendering and UI geometry must be visually verified on Plus/SP.
Maintain Beta 2.4.2/2.4.3 rollback installers and do not publish until distribution rights and binary provenance are confirmed.
