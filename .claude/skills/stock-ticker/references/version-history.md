# Stock ticker — version history

Fix log for the Flip Clock stock ticker. Append a new entry for every
stock-related change: problem, root cause, fix, result, key files.

Last updated: 2026-06-03

## v1 — Initial feature (2026-06-03)

- Stock ticker support for 3 symbols, with API key storage
- `/api/stock-config` GET and POST endpoints
- Stock bar on the Flip Clock screensaver
- Periodic refresh every 5 minutes

Key files: `src/stock.h`, `src/stock.cpp`, `src/wifi_manager.cpp`,
`src/ui/ui_screensaver.cpp`, `src/web_server.cpp`

## v2 — TLS memory failure fix (2026-06-03)

**Problem:** HTTPS requests failed with a TLS allocation error on device
(`-32512`).

**Fix:** Switched from `WiFiClientSecure` + HTTPS to `WiFiClient` + HTTP for the
Twelve Data endpoint.

**Result:** Stock fetch became stable within device memory limits.

Key files: `src/stock.cpp`

## v3 — Immediate fetch after save (2026-06-03)

**Problem:** Saving stock config did not fetch immediately — it needed uptime
greater than 5 minutes.

**Fix:** In `POST /api/stock-config`, force the next poll immediately:
`lastStockUpdate = millis() - STOCK_UPDATE_MS - 1;`

**Result:** New symbols and key fetch on the next network loop tick.

Key files: `src/web_server.cpp`

## v4 — Unavailable-symbol UI and logging (2026-06-03)

**Problem:** `AOT.BK` returned no price on the free tier and the UI was
ambiguous about why.

**Fix:** UI shows `SYM N/A` with a grey recolor for invalid entries; logs now
carry the API error code and message per symbol. Removed a temporary verbose
response-body debug print.

**Result:** Valid data and unavailable data are visually distinct.

Key files: `src/ui/ui_screensaver.cpp`, `src/stock.cpp`

## v5 — Screensaver rebuild invalidation (2026-06-03)

**Problem:** A screensaver built while stock was disabled kept a stale layout
after the feature was enabled from the web portal.

**Fix:** Added `invalidate_screensaver_build()` to force a rebuild on next show,
called from `POST /api/stock-config`.

**Result:** The stock bar appears and disappears correctly after config changes.

Key files: `src/ui/ui_screensaver.cpp`, `src/ui/ui_screens.h`,
`src/web_server.cpp`
