# MustardTube v0.9.11 Beta 2.2 — Radio metadata and background playback experiment

**Historical Beta 2.2 experiment.** The unvalidated/source-only statements below do not describe Beta 2.2.6a. See the [current release notes](RELEASE-0.9.11-BETA2.2.6A.md).


**Source candidate only — NOT a ready-to-install `.muxapp`.**
Based on the supplied **Beta 2.1 MultiDisplay** source. All existing YouTube extraction and video-resolution code stays unchanged. Primary devices: RG35XX Plus and RG35XX SP on **muOS 2606.0 Andromeda**. No Goose/Jacaranda support and no PortMaster requirement for end users.

## Design

1. **A** on a radio station: foreground Wasabi, as before. MustardTube now writes a temporary named `.m3u` playlist containing `#EXTINF` station metadata, calls Wasabi `--live --title ...`, and falls back to the direct stream URL only if Wasabi rejects the playlist. Temporary playlists are removed after the player exits. This may improve station naming, but **visualiser/track text in Wasabi is controlled by Wasabi**; black-screen behaviour can only be verified on a handheld.
2. **START** on a station: request a separate audio-only background player. It runs with `-nodisp -vn` and `SDL_VIDEODRIVER=dummy`, rather than trying to leave Wasabi's fullscreen graphics running under a game. MustardTube reopens Radio and shows `BG RADIO: <station>` when the audio player's process is active. Exit MustardTube with **B** to return to muOS and select a game.
3. **SELECT** on Radio: stop background radio. This action must be used from MustardTube after leaving a game; there is **no global in-game hotkey** in this experiment.
4. Starting foreground Wasabi **stops** the independent background radio first, preventing two copies of the station.

The short-lived Node controller is `scripts/background-radio.js`. It spawns a **separately bundled ARM64 `ffplay-radio` audio-only binary**, in its own process group, then exits. The background audio decoder, not Node, remains resident. Status files store station names, process IDs, process start times and binary path, **not streaming URLs**. Start selection files are deleted immediately. Stop validates PID start time, process-group identity and executable path before signaling; it never performs a generic `pkill` or a broad process kill.

## Critical requirements before hardware validation

- The provided **source ZIP contains no compiled `ffplay-radio`, SDL UI binary or prebuilt runtime package**. The normal Beta 2.1 prebuilt release is needed to reuse its established Node and yt-dlp runtime. It is **not** valid to distribute this archive as an installer.
- Developer should compile the main UI with `bash scripts/build-arm64-ubuntu.sh` on the prepared ARM64 cross-compile VM.
- Obtain an auditable FFmpeg source checkout/tarball (respect its licensing). In the VM, build the audio-only player with `bash scripts/build-background-player-arm64.sh /path/to/ffmpeg/source`. Verify its linked libraries against *stock Andromeda*; an ARM64 binary built on Ubuntu is not automatically portable to the handheld. The helper restricts enabled decoders to common internet-radio codecs, but its exact FFmpeg-version compatibility and runtime linkage have **not** been validated here.
- Obtain the known-good **Beta 2.1 prebuilt `.muxapp`**. Once both ARM64 binaries are built and dependencies verified, run `bash scripts/package-beta22-from-beta21-installer.sh /absolute/path/to/Beta2.1.muxapp`. This requires licensing information for bundled FFmpeg.
- If `bin/ffplay-radio` is absent or not ARM64, the controller fails safely. Video and ordinary Wasabi radio playback remain available. Zero-configuration background playback is achieved **only when the player and its required libraries are actually bundled / confirmed stock**, not by asking end users to install anything.

## Tested off device in this source candidate

- Background controller start/status/stop with a fake audio player; tests include separate process group, stored station name without URL, missing player and unsafe URL rejection.
- Foreground Wasabi handoff, named playlist and deletion with a fake Wasabi process.
- Existing video resolver, radio directory, cache and display compatibility regressions (network-free mocks).

**Still unverified:** real player startup and network audio, FFmpeg protocol/codec coverage, actual Wasabi radio title rendering, concurrent audio mixing with Pickles and RetroArch, survival of the muOS foreground-launch cleanup, battery/lid behaviour on RG35XX SP, audio glitches on demanding systems and pause/stop behaviour on shutdown. Do not present this as an accepted public beta until those tests pass.

## Hardware test checklist (keep Beta 2.1 as rollback)

1. Verify foreground A playback on an MP3 and AAC radio station. Confirm whether Wasabi shows station name/visualiser; test A's fallback to direct stream when playlist is refused.
2. Press START on an MP3 radio station. Verify `BG RADIO: <station>` appears and audio plays while MustardTube Radio remains displayed. If silent, inspect the redacted launch log and check linked libraries / PipeWire audio routing before attempting game launch.
3. Leave MustardTube with B, start a lightweight Game Boy / SNES title in Pickles, and test whether radio plus game audio can be heard; then repeat with RetroArch. Check latency, framerate, heat and volume.
4. Return to MustardTube > Radio, press SELECT to stop. Verify no audio remains and all ffplay-radio processes exit. Try selecting another station and switching back to foreground Wasabi.
5. Test network loss, invalid stations, long sessions, lid closing on RG35XX SP, and power cycling. Make sure no orphaned background process or endless restart occurs. The current beta **does not** promise reconnection or in-game playback controls.

## User controls

| Location | Button | Action |
| --- | --- | --- |
| Radio country list | A | Open station list |
| Radio station/favourites | A | Foreground Wasabi (named M3U, fallback to direct) |
| Radio station/favourites | START | Experimental background radio |
| Anywhere in Radio | SELECT | Stop experimental background radio |
| Radio | B | Previous page / leave app; audio keeps playing if process survives |
| Radio | X / Y | Existing page refresh / favourite actions, unchanged |
| PC test UI | S / K | Background start / stop |

Note: Wasabi already offers audio visualisers and cover art settings in **MENU > Settings > Display**; these may not appear for every live stream. Track metadata depends on the radio station, and the temporary M3U helps label it rather than supplying real-time song data.
