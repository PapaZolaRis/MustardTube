# MustardTube branding artwork

The PNG icons are cropped from the logo concept explicitly selected by the
MustardTube maintainer for this release. `glyph/mustardtube.png` uses the dark
variant (256×256); `grid/mustardtube.png` uses the yellow variant (160×160).
Alternate icon tiles and the wordmark reference are retained here for future
use. Their inclusion does not change the copyright or licenses of upstream
third-party artwork. Preserve attribution and provenance with redistributed
artwork, subject to the permissions applicable to the artwork itself.

Actual YouTube video thumbnails and radio-station logos are not overwritten,
branded, or watermarked by MustardTube.

## Beta 2.3.1 display assets

- `header-dark.png` (96×96): approved icon for dark in-app themes.
- `header-paper.png` (96×96): approved yellow icon for Paper.
- `../glyph/mustardtube.png` (256×256): transparent white alpha-mask, meant
  for muOS frontend tinting, **not** the coloured in-app header.
- `../grid/mustardtube.png` (160×160): unchanged full-colour grid artwork.

Avoid using the app icon as a placeholder or overlay on YouTube thumbnails.

## Beta 2.3.2 transparent header mascot

`header-dark.png` and `header-paper.png` now have a fully transparent
background and no enclosing app-icon tile. The Paper variant uses a dark
outline so it reads cleanly on ivory. Both PNGs are 96x96 RGBA and are
loaded only on first use, then cached. Acid Bloom shares the dark asset.
The muOS launcher glyph and actual video thumbnails are unchanged.
