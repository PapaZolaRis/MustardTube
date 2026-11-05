MustardTube v0.9.1 integration notes

The native muOS application launcher uses # ICON: mustardtube.
The PNG in MustardTube/glyph/mustardtube.png is sufficient for the per-application
list glyph according to current muOS Andromeda application runner conventions.
Theme grid artwork is a secondary fallback and can differ between themes.

Playback uses Wasabi /opt/muos/frontend/muxmedia. Stream URL resolution must
return one combined video+audio stream. 360p is a preference, not a universal
YouTube format guarantee. Widescreen video must retain aspect ratio.

Local network and startup tests have not run in this source-build environment.
Check launch.log and timing.log if any playback attempt fails. Existing backend
mocks cannot predict real YouTube resolver outcomes.
