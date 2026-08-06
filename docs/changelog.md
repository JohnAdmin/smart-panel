# Changelog

[← Docs index](README.md)

Per-release build notes also live in [`UPDATE_LOG.md`](../UPDATE_LOG.md).

## 2026-06-03 — Stock Ticker (Twelve Data)

**feat: add stock ticker (Twelve Data) on flip-clock screensaver**

- `src/stock.h` + `src/stock.cpp` — fetch 3 configurable symbols via Twelve Data API (batch quote endpoint, 5-min interval)
- `src/config.h` — added `STOCK_UPDATE_MS 300000` constant
- `include/globals.h` — `#include "../src/stock.h"` for universal access
- `src/wifi_manager.cpp` — `loadStockConfig()` on boot; immediate fetch on WiFi connect; 5-min refresh loop
- `src/ui/ui_screensaver.cpp` — compact 3-label stock bar above status bar in Flip Clock (Style 0) screensaver
- `src/web_server.cpp` — **Stock Ticker** settings tab: enable toggle, 3 symbol inputs (supports Thai SET `.BK` suffix and US stocks), API key with mask; `/api/stock-config` GET + POST endpoints (NVS-backed)
- `platformio.ini` — restored `extra_scripts = post:fix_esptool.py` (was corrupted)
