# Stock Ticker Runbook and Version History

Last updated: 2026-06-03

## Purpose

This document is the single source of truth for:
- What was changed in the stock ticker feature
- Why each fix was made
- How to verify behavior quickly
- How to avoid repeating the same issues

## Scope

- Platform: WT32-SC01 Plus (ESP32-S3)
- Feature: Stock ticker on Flip Clock screensaver (Style 0)
- API: Twelve Data quote endpoint

## Architecture Snapshot

- Data fetch and parsing:
  - `src/stock.cpp`
  - `src/stock.h`
- Polling and trigger logic:
  - `src/wifi_manager.cpp`
- UI rendering:
  - `src/ui/ui_screensaver.cpp`
  - `src/ui/ui_screens.h`
- Web configuration API and portal:
  - `src/web_server.cpp`
- Constants and globals:
  - `src/config.h`
  - `include/globals.h`

## Version History

## v1 - Initial Feature (2026-06-03)

- Added stock ticker support for 3 symbols and API key storage
- Added `/api/stock-config` GET and POST endpoints
- Added stock bar to Flip Clock screensaver
- Added periodic refresh every 5 minutes

Key files:
- `src/stock.h`
- `src/stock.cpp`
- `src/wifi_manager.cpp`
- `src/ui/ui_screensaver.cpp`
- `src/web_server.cpp`

## v2 - TLS Memory Failure Fix (2026-06-03)

Problem:
- HTTPS requests failed with TLS allocation error on device (`-32512`)

Fix:
- Switched from `WiFiClientSecure` + HTTPS to `WiFiClient` + HTTP for Twelve Data endpoint

Result:
- Stock fetch became stable in device memory limits

Key files:
- `src/stock.cpp`

## v3 - Immediate Fetch After Save (2026-06-03)

Problem:
- Saving stock config did not fetch immediately (needed uptime > 5 minutes)

Fix:
- In POST `/api/stock-config`, force next poll immediately:
  - `lastStockUpdate = millis() - STOCK_UPDATE_MS - 1;`

Result:
- New symbols and key fetch on next network loop tick

Key files:
- `src/web_server.cpp`

## v4 - Unavailable Symbol UI and Logging (2026-06-03)

Problem:
- AOT.BK did not return price on free tier, UI looked ambiguous

Fix:
- UI shows `SYM N/A` with gray recolor for invalid entries
- Logs now include API error code and message per symbol
- Removed temporary verbose response-body debug print

Result:
- Clear differentiation between valid data and unavailable data

Key files:
- `src/ui/ui_screensaver.cpp`
- `src/stock.cpp`

## v5 - Screensaver Rebuild Invalidation (2026-06-03)

Problem:
- If screensaver was built while stock was disabled, enabling from web could leave stale layout

Fix:
- Added `invalidate_screensaver_build()` to force rebuild on next show
- Called invalidation from POST `/api/stock-config`

Result:
- Stock bar appears/disappears correctly after config changes

Key files:
- `src/ui/ui_screensaver.cpp`
- `src/ui/ui_screens.h`
- `src/web_server.cpp`

## Known External Limitation

- Twelve Data free tier does not reliably provide Thai SET symbols (`.BK`) in this project context
- Typical error seen:
  - `code=404 symbol not found: AOT.BK`
  - or plan-related errors depending on account/endpoint behavior

Implication:
- `AOT.BK` can show as `N/A` while other symbols (for example `AAPL`, `XAU/USD`) are valid

Workarounds:
- Use supported symbols on free tier
- Upgrade Twelve Data plan
- Integrate an alternative API for Thai SET data

## Verification Checklist

## After Flash/Reboot

- Expect temporary `N/A` until WiFi reconnect and first fetch complete
- Confirm serial contains:
  - `[STOCK] GET http://api.twelvedata.com/quote?...`
  - valid lines for supported symbols

## After Saving Config in Web Portal

- POST `/api/stock-config` should:
  - persist NVS values
  - invalidate screensaver build cache
  - trigger immediate fetch cycle

Expected behavior:
- Stock bar layout updates without manual style switch
- Valid symbols show price and change
- Invalid symbols show gray `N/A`

## Quick Troubleshooting

## Symptom: All symbols show N/A

Check in order:
1. Wait 20-30 seconds after boot (first fetch latency)
2. Confirm WiFi connected
3. Confirm API key length and value
4. Check serial for `[STOCK]` logs
5. Verify symbols are supported by Twelve Data plan

## Symptom: No stock bar visible

Check in order:
1. Screensaver style is 0 (Flip Clock)
2. Stock ticker enabled in web config
3. Save config again (triggers rebuild invalidation)

## Symptom: Only AOT.BK fails but others work

- Expected with current provider limitations
- Replace with a supported symbol or change data source/plan

## Recommended Team Workflow

- For every stock-related fix:
  - Append a new version entry in this file
  - Include problem, root cause, fix, result, key files
- Keep serial log examples brief and actionable
- Keep this file aligned with firmware behavior after each release
