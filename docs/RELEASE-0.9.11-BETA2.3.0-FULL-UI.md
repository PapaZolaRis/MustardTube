# MustardTube v0.9.11 Beta 2.3.0 — full lightweight UI candidate

## Scope
- Shared sidebar and mustard-yellow/dark header on Home, Search, Recent,
  Radio Directory, Radio Now Playing, Settings, About, Video Details and Results.
- Uniform video cards: all **477 × 54 px**, thumbnails **86 × 48 px**;
  four visible Home cards and four visible Results cards. No featured card.
- Search keyboard, radio list, volume sliders, Settings selectors and About
  manual fitted to the 640 × 480 logical display. Associated pointer hitboxes
  have been updated to match. Existing controller mappings are preserved.
- Existing app icons and Midnight theme are included using the installed
  paths verified by the packaging script.

## Stability and performance guardrails
- The YouTube backend, radio backend, Wasabi handoff and runtime binaries are
  **unchanged**. No additional libraries, background processes, animations,
  textures, or artwork preloads.
- Max four thumbnail textures per Home or Results view, with existing cache
  and release behavior retained. Existing SDL2 event-driven drawing retained.
- Existing cache/favourites/preferences storage unchanged.

## Validation and release status
- Static layout contract: `python3 scripts/test-beta230-layout.py`.
- Backend/radio regression tests run through `build-package.sh`.
- Compile/link and packaging must be completed on the Ubuntu ARM64 toolchain.
- Hardware tests on RG35XX Plus and SP required. This is a candidate, not a
  stable release. Contributor rights and bundled-dependency source obligations
  still require separate review before publication.
