# MustardTube v0.9.11 Beta 2.2.9a — visual asset correction

SPDX-License-Identifier: GPL-3.0-only

This candidate corrects two asset-path packaging errors discovered after the
first Beta 2.2.9 test: an installed glyph lives at `MustardTube/glyph/`, not
inside `MustardTube/assets/glyph/`; the Midnight theme must be copied into
`MustardTube/assets/themes/` by the builder. The GUI now loads the installed
glyph without duplicating the PNG on disk, and the package builder verifies
that the actual resulting ZIP contains the correct glyph, grid icon and theme.

The 640x480 Home sidebar and four-video layout from Beta 2.2.9 remain intact.
Other screens are *not* redesigned in this hotfix. Playback, radio, search and
controller paths are unchanged. A new ARM64 build and Plus/SP tests are required.
Third-party licensing and contributor-permission issues remain open.
