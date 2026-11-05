# MustardTube v0.9.10 RC1 — cache maintenance update

This is a backend-only maintenance patch for the **working** v0.9.10 RC1.
The ARM64 application executable, launcher, Wasabi integration and settings are unchanged.

## User control

From the **Style** tab, press **X — Clear cache**. The app exits the SDL
renderer, runs cleanup, then returns to Style. The backend records a
small status message in `data/status.txt` including an approximate amount
of reclaimed temporary storage.

Cache cleanup may remove cached Home and Search results; they will be
reloaded when online. The selected theme, search query and quality choice
are preserved. Saved/downloaded video files and user history are not
cleaned by the cache action (download support is not included here).

## Automatic safeguards

- yt-dlp disk cache: reset if over 32 MiB on the next backend operation.
- JPEG thumbnails: no more than 8 local thumbnails, each at most 160,000 bytes,
  with orphaned/oversized entries pruned on each backend operation.
- launch/timing logs: truncated to latest 200 lines after exceeding 128 KiB.
- abandoned temporary metadata and stale HLS manifests: periodically removed.
- no background daemon or additional runtime dependency.

## Debugging

On the device, the following command reports approximate disposable cache size:

```sh
MUSTARDTUBE_APP_DIR=/run/muos/storage/application/MustardTube \
sh /run/muos/storage/application/MustardTube/scripts/youtube-backend.sh cache-size
```

## Safety

The clear-cache action affects only MustardTube-owned disposable data,
not arbitrary files, ROM directories, media libraries, settings, or TF2.
Do not invoke cleanup concurrently with an active MustardTube backend worker.
The built-in Style screen uses the supervisor so that cleanup occurs after
its workers stop.
