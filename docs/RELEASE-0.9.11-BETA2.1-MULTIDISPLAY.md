# MustardTube v0.9.11 Beta 2.1 — Multi-Display Compatibility Test

**Developed by PapaZolaRis**  
**Platform:** muOS 2606.0 Andromeda (ARM64)  
**Release status:** Experimental prerelease — hardware validation requested

MustardTube combines lightweight YouTube browsing and Internet Radio with the stock muOS Wasabi player. This **narrow Beta 2.1 patch** aims to let the same application UI fit different Andromeda-supported screen shapes without distortion or clipped navigation. The **existing Beta 2 remains the rollback**.

## Changes since Beta 2
- Detect the native screen dimensions with SDL2.
- Maintain the unchanged 640x480 layout on original 640x480 devices.
- Auto-fit the interface proportionally on 3:2, square 1:1, 4:3 and widescreen displays (16:9) without stretching thumbnails or controls.
- Adjust mouse/touch selection for letterbox margins.
- Fix native-resolution screenshots so readback buffers match the physical renderer output.
- Fall back to the previous window size if native fullscreen cannot be created.
- Write a safe display geometry diagnostic to `MustardTube/data/launch.log`.

**No changes:** YouTube search/download metadata backend, 360p/480p playback paths, experimental recorded 720p, Internet Radio backend, caching, 6 Home categories, Search video cards, controller navigation, Wasabi launcher, or bundled runtime components.

## Device qualification

muOS 2606.0 Andromeda officially lists 18 devices: Anbernic RG Vita Pro, RG28XX H, RG34XX H, RG34XX SP, RG35XX 2024, RG35XX H, RG35XX Plus, RG35XX Pro, RG35XX SP, RG40XX H, RG40XX V, RG CubeXX, RG SP; BatleXP G350; GKD Pixel 2; TrimUI Brick, Brick Pro and Smart Pro.

**This release has not been tested on all these devices.** Only the UI display fit has automated coverage; successful YouTube playback, station playback, controller mappings and binary runtime dependencies still need confirmation on the actual hardware. In particular, some models require an external Wi-Fi adapter.

## Installation

1. Download the `.muxapp` attached under **Assets**.
2. Back up your existing MustardTube `data` directory if updating.
3. Copy the `.muxapp` to `ARCHIVE` on the SD card.
4. On the handheld open **Applications → Archive Manager** and install.
5. Launch MustardTube and test the UI, Wi-Fi, playback, Radio and cache controls. Do not remove the previous Beta 2 installer until you have verified your device.

## How to report compatibility

Submit a GitHub issue at https://github.com/PapaZolaRis/MustardTube/issues and provide: (a) device model and screen shape/resolution; (b) firmware version; (c) whether launch, Home, Search, Radio and Wasabi work; (d) whether borders, cropping, input mapping or readability are problematic; (e) any steps to reproduce.

If available, include the `MustardTube display:` line from `MustardTube/data/launch.log`, but **redact personal data, streaming tokens, and signed URLs** from any shared logs.

See the packaged `docs/DISPLAY-COMPATIBILITY-BETA2.1.md` for screen fit examples and a detailed checklist.

## Important limitations

- Widescreen and square layouts intentionally preserve a centered 4:3 UI with margins, rather than redesigning the entire interface to fill the display.
- Performance and font readability on high-resolution devices need testing.
- Some devices have no built-in Wi-Fi and need external connectivity.
- Recorded 720p is experimental and is not seekable; live streams are not seekable.
- Not affiliated with YouTube, Google, Anbernic or MustardOS. The application is provided **AS IS**, use at your own risk, to the fullest extent permitted by applicable law; comply with copyright and service terms.

**Developer:** PapaZolaRis
