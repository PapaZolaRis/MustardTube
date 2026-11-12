# Publication gate: GPL-3.0-only relicensing

This source archive is a **prepared migration**, not proof of rights clearance.
The archive contains no `.git` directory or contributor history. Although the
maintainer identifies as the original MustardTube developer, the existing
`Copyright (c) 2026 MustardTube contributors` notice does not by itself show
that the maintainer can relicense every contribution.

Before publishing Beta 2.2.8 as GPL-3.0-only:

1. In the actual Git repository run `git status --short`, `git log --format='%h %an <%ae> %s'`,
   `git shortlog -sne --all`, and examine contributor/license history. Preserve
   any uncommitted changes while applying this prepared tree.
2. Confirm rights or permission for each original MustardTube contributor's
   first-party C, shell, JavaScript, artwork and documentation contributions.
   Obtain permission or exclude/replace contributions if ownership is unclear.
   Do not simply overwrite another author's license notice.
3. Verify original artwork ownership/permission for the new logo; it is based
   on the maintainer-approved concept. Verify rights to redistribute files
   not authored by the maintainer, including the demo MP4 fixture.
4. Review `docs/THIRD-PARTY-LICENSE-REVIEW.md` and obtain matching source for
   each bundled GPL/LGPL or other source-obligated compiled binary, including
   FFmpeg/ffplay and the exact packaged yt-dlp/runtime dependencies.
5. Cross-compile the modified source and package it; source and actual binaries
   **must correspond**. The existing Beta 2.2.7 installer binary still displays
   an MIT notice and must not be presented as a GPLv3 Beta 2.2.8 build.
6. Publish exact Corresponding Source alongside any public `.muxapp`, with
   conspicuous access instructions. Include licensing notices inside package.
7. Test application, About screen, and icons on both RG35XX Plus and SP.
8. Check original Git diff, retain prior MIT releases as historical artifacts,
   and do not publish until all release-blocking items are resolved.
