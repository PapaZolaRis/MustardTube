# MustardTube v0.9.2 — search diagnostics and honest Home

Target: **muOS Andromeda 2606.0**, RG35XX Plus/SP.

- No unverified Trending feed or hardcoded video cards. Home offers Search and Last Results; Recent exposes the previous search.
- Search command remains identical to v0.9.1; failed searches now classify errors without storing signed URLs, search terms, or raw extractor output.
- Video-format fallback, quality preference, cache clearing, logo, pagination, and Wasabi return logic retained.
- **Not a confirmed fix for the on-device search regression.** The failure can be caused by yt-dlp, network, TLS, or JS runtime. Capture `search_failure category=` and the diagnostics command before choosing a targeted remedy.
- Does not change firmware, runtime libraries or the user installation process.
