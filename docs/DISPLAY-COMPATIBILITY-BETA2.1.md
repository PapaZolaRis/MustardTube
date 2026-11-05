# MustardTube 0.9.11 Beta 2.1 — multi-display compatibility candidate

**Developer:** PapaZolaRis  
**Firmware:** muOS 2606.0 Andromeda only  
**Status:** experimental / community hardware qualification

This release is a narrow compatibility update to the community Beta 2. The original YouTube backend, Radio directory, Wasabi launcher, themes, external runtime, caching and 360p/480p/experimental-720p handling remain unchanged. Do not confuse screen scaling with proof of compatibility across different SoCs.

## Changes

- Detect native screen dimensions through the stock SDL2 display API.
- Keep the original working 640x480 layout unchanged on 640x480 devices.
- On other panel sizes, request the native fullscreen display mode and let SDL scale a **640x480 logical UI** into a centered aspect-preserving viewport. No cropping or distorted video cards; unused margin is expected on square and widescreen displays.
- Correct mouse/touch coordinate mapping when letterboxing adds margins.
- Allocate screenshots using the renderer's **physical output size**, preventing a 640x480-sized readback buffer on larger displays.
- Keep a safe 640x480 window fallback if native fullscreen cannot be created.
- Log the requested/native, window and viewport dimensions in `data/launch.log` for device diagnostics.
- No new UI runtime, no additional media decoder, no PortMaster, no new background service and no system-wide governor changes.

## Screen fit examples (logical 640x480 UI)

| Panel | Display shape | Centered content area | Unused margins |
|---|---|---|---|
| 640x480 | 4:3 | 640x480 | None |
| 720x480 | 3:2 | 640x480 | 40px left/right |
| 720x720 | 1:1 | 720x540 | 90px top/bottom |
| 1024x768 | 4:3 | 1024x768 | None |
| 1280x720 | 16:9 | 960x720 | 160px left/right |
| 1920x1080 | 16:9 | 1440x1080 | 240px left/right |

The initial screen fit deliberately **does not extend the interface to fill square/wide panels**. This is a performance-conscious beta safety measure. High-resolution display scaling may have different resource costs depending on each device's SDL renderer.

## Official muOS 2606.0 Andromeda device families

The [official device list](https://muos.dev/devices) currently shows **18 devices**:

- **Anbernic (13):** RG Vita Pro; RG28XX H; RG34XX H; RG34XX SP; RG35XX 2024; RG35XX H; RG35XX Plus; RG35XX Pro; RG35XX SP; RG40XX H; RG40XX V; RG CubeXX; RG SP.
- **BatleXP (1):** G350.
- **GKD (1):** Pixel 2.
- **TrimUI (3):** Brick; Brick Pro; Smart Pro.

**Supported by muOS does NOT mean tested with MustardTube.** This version adds display-size handling, but video/radio streaming, SDL2 controller maps, native yt-dlp/Node runtimes, dynamic libc and Wasabi integration still require device-specific qualification.

**Networking:** RG28XX H, RG35XX 2024, BatleXP G350 and GKD Pixel 2 are listed by muOS as requiring **external Wi-Fi**. They cannot browse online content without working connectivity. Other hardware may require its own network setup.

## Acceptance checklist for testers

1. Install the beta on **muOS 2606.0 Andromeda** with stock Wasabi. Back up the old MustardTube `data/` directory first. If upgrading, preserve user preferences/favourites.
2. Confirm launch, visible Home header, all six tabs, legible text, no clipped cards, and usable controls. Note any black/unused panel margins; those are expected outside the 4:3 canvas.
3. Confirm Radio country and station names are alphabetically sorted; hold Up/Down to scroll; A/B and LB/RB respond; station logo/flag cache does not block scrolling.
4. Confirm Search shows actual thumbnails; X loads more results repeatedly; Home category switching works.
5. Verify a recorded 360p and 480p video plays with audio and returns from Wasabi; verify a compatible radio station; do **not** assume live streams are seekable or recorded 720p seeking works.
6. In Settings, verify Clear Cache leaves saved radio favourites intact. Reopen About and scroll the guide.
7. Capture a screenshot on the actual device and check image dimensions. Beta 2.1 screenshots should use physical display dimensions. If screenshot saving fails, report it without publishing private log content.
8. For diagnostics, inspect the `MustardTube display:` line in `MustardTube/data/launch.log`. Record the device model, SDL window size, muOS build and failure steps.
9. Report whether performance remains acceptable on your device; especially note RG Vita Pro, TrimUI Smart Pro, TrimUI Brick/Pro, RK-based devices and devices needing an external Wi-Fi dongle.

**Reporting:** Open a GitHub issue at https://github.com/PapaZolaRis/MustardTube/issues. Never upload unredacted signed stream URLs, credentials or sensitive data.

## Release qualification boundary

The cross-compiled ARM64 binary and automated source tests are **not substitutes for physical device tests**. Beta 2.1 is a compatibility test candidate, **not a declaration that all 18 devices are supported**. Users who find a compatibility issue should continue using Beta 2 and report the model/logs.
