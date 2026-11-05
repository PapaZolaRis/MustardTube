# Upload MustardTube to GitHub using only a browser (Windows)

You do **not** need Git, GitHub Desktop, a terminal, or an API token.

## A. Create a repository

1. Sign in at https://github.com .
2. Open https://github.com/new .
3. Repository name: `MustardTube` (or `mustardtube`).
4. Description: `Lightweight YouTube and Internet Radio client for muOS Andromeda on H700 handhelds`.
5. Visibility: `Public` when ready for community feedback.
6. **Do not** add a GitHub-generated README, `.gitignore` or licence: the source ZIP already contains all three.
7. Select **Create repository**.

## B. Upload source files (not the ZIP itself)

1. Download the `MustardTube-v0.9.11-Beta2-GitHub-Source-WEB-UPLOAD.zip` file.
2. In Windows File Explorer, right-click ZIP → **Extract All**.
3. Enter the extracted folder. You should see `README.md`, `LICENSE`, `src`, `scripts`, `assets`, `docs` and `packaging` at the same level.
4. On the new GitHub repository page, choose **uploading an existing file**, or **Add file → Upload files**.
5. Select all files and subfolders *inside* the extracted folder and drag them into the browser upload area. Don't upload the ZIP directly.
6. Commit message: `Initial source release: MustardTube v0.9.11 Beta 2`.
7. Select **Commit changes** (or **Propose changes** if that is the current UI wording).
8. Confirm the repository root displays `README.md` and the `src`, `scripts`, `assets`, `docs` folders. If you see an extra `mt-github-web-ready` folder at the top, the upload was one level too high.

GitHub allows up to 100 files per browser upload, each at most 25 MiB. This
source package is below both limits. The prebuilt `.muxapp` is much larger and
must be attached as a release asset instead.

## C. Upload the prebuilt device installer as a prerelease

1. From the repository page select **Releases → Create a new release** (or **Draft a new release**).
2. Create tag `v0.9.11-beta.2` on the `main` branch.
3. Title: `MustardTube v0.9.11 Beta 2 — muOS Andromeda (H700)`.
4. Paste the forum draft or the supplied `GITHUB-RELEASE-NOTES.md` into the release description.
5. Drag the separate 85 MB
   `MustardTube-v0.9.11-Combined-Beta2-RadioUX-Andromeda-ARM64.muxapp`
   into the **Attach binaries** / release assets area (not into the code file list).
6. Mark **Set as a pre-release**, not a stable/latest release.
7. Select **Publish release** (or save draft first if not ready).

## D. Share with muOS community

Paste your GitHub **repository URL** and GitHub **Releases page URL** into the
muOS **Applications → Testing** forum post. Attach screenshots and request
feedback via GitHub Issues.

Do not upload SSH keys, device logs with signed YouTube URLs, user history, or
personal files into the public repository.
