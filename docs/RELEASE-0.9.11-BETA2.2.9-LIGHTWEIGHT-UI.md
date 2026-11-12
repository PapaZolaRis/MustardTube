# MustardTube v0.9.11 Beta 2.2.9a — Home UI candidate

SPDX-License-Identifier: GPL-3.0-only

## Scope

- Compact left-side Home navigation at the existing 640×480 logical resolution.
- Larger featured video card with three secondary cards; four visible video
  thumbnails remain the maximum.
- Reuse the approved app glyph in the header, loaded once per process.
- Update the Midnight theme toward mustard yellow and charcoal; Forest/Paper
  theme files remain unchanged.
- Correct mouse hit-testing to match new Home sidebar, categories and cards.
- Keep controller LB/RB tab switching, category changes and UP/DOWN selection.
- Keep other screens' existing layouts pending a separate usability review.

## Stable architecture / resource rules

- No additional dependencies, SDL render targets, network requests, or threads.
- No extra thumbnails, continuously running animations, or idle polling.
- Background image remains cached and normal dirty-frame rendering applies.
- Home results, selection actions, video preparation, radio backend and Wasabi
  handoff are unchanged.
- Safe rollback remains the validated Beta 2.2.7 installer.

## Release gate

This is source-only until compiled in Ubuntu. Run the included backend, mixer,
cache, layout and GPL checks. Verify appearance, sidebar mouse hit-testing,
controller navigation, video playback, radio, cache and multi-display behaviour
on the Plus and SP. Contributor rights and bundled-source compliance remain
unresolved; do not publish an official GPL binary prematurely.
