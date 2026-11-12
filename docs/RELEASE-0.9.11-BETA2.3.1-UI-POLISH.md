# MustardTube v0.9.11 Beta 2.3.1 — UI/UX refinement candidate

## Visual and packaging changes only

- **muOS launcher glyph** is now a transparent monochrome alpha-mask, preserving
  the approved MustardTube symbol while allowing the active muOS theme to tint
  its shape rather than an opaque square. `# ICON: mustardtube` is unchanged.
- **In-app header** uses the approved dark design for Midnight/Forest and a
  brighter approved yellow tile for Paper. Images are pre-scaled to 96×96 and
  lazily loaded once, so there is no large image kept alive unnecessarily.
- **All three themes** have explicit accent-fill, accent-ink and on-accent
  foreground colours. Paper no longer puts light yellow text on a light
  selection. Midnight has charcoal surfaces; Forest keeps dark green.
- **Packaging** now ships all three theme files and both header icon variants
  to their runtime paths; the builder compares the packaged bytes to source.
- **Footer** uses two concise rows for page actions and page switching without
  changing any controller assignment.
- **Backdrop** uses a flat SDL fill instead of a gradient texture with large
  circles. This avoids allocating a ~1.17 MiB 640×480 backdrop texture and
  removes decorative noise, especially on Paper.
- **Equal-size cards** remain 477×54, with bounded four-thumbnail rendering.
  Original thumbnails remain unchanged; no overlays or watermarks.

## Preserved and intentionally out of scope

The YouTube extractor, Wasabi handoff, video resolutions and seeking, radio
backend, game/radio audio mixer, radio volume hotkeys, cache handling and
controller mappings remain unchanged from Beta 2.3.0. There are no new
libraries, network requests, workers or animations.

## Validation status

Source-only tests check theme contrast, PNG transparency, source/runtime asset
paths, 640×480 bounds, equal card dimensions, core file hashes, and package
script syntax. Build on an Ubuntu ARM64 cross compiler and perform device
verification on RG35XX Plus and SP before calling the result hardware-tested.

The prior GPLv3 contributor-ownership and third-party corresponding-source
compliance review remains open; this source candidate is not a public release.
