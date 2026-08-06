# Stock Ticker — Setup & Testing

[← Docs index](README.md)

The Flip Clock screensaver can show a compact 3-symbol quote bar, fed by the
Twelve Data API.

## 1. Get a Twelve Data API Key (Free)

1. Go to [twelvedata.com](https://twelvedata.com) → Sign Up (free)
2. Dashboard → **API Keys** → copy the key
3. Free tier: **800 API calls/day**, **8 req/min** — more than enough (panel fetches every 5 minutes)

## 2. Configure via Web Portal

1. Open `http://smartpanel.local` (or the panel's IP address)
2. Go to the **Stock Ticker** tab
3. Fill in:
   - **Enable Stock Ticker** — toggle ON
   - **Symbol 1/2/3** — enter ticker symbols, e.g.:
     - Thai SET: `AOT.BK`, `PTT.BK`, `KBANK.BK`
     - US stocks: `AAPL`, `MSFT`, `NVDA`
     - Metals/Crypto: `XAU/USD` (gold), `BTC/USD`
     - Leave blank to skip a slot
   - **API Key** — paste your Twelve Data key
4. Click **Save & Apply**
5. The panel fetches new data within ~10 seconds (next WiFi loop tick)

## 3. Verify on Screensaver

1. Let the panel idle for 2 minutes (or reduce screensaver timeout in Settings)
2. The **Flip Clock** screensaver will show a compact stock bar above the status strip
3. Each slot displays: `SYM  price  ±x.xx%` — green for up, red for down
4. If market is closed, a ` z` suffix is appended (e.g. `AOT  45.25  +1.20% z`)
5. Labels update every 5 minutes while WiFi is connected

## 4. Troubleshoot

Symptom-by-symptom diagnosis, the `[STOCK]` serial log reference, and the v1–v5
fix history live in [`STOCK_TICKER_RUNBOOK.md`](../STOCK_TICKER_RUNBOOK.md),
which routes to `.claude/skills/stock-ticker/`. They are kept there so they stay
in one place and get updated with the firmware.

Quick orientation: `SYM N/A` in grey means the symbol is configured but the API
returned nothing for it; `--` means the slot is empty or not yet populated.

## Implementation

`src/stock.cpp` / `src/stock.h` (fetch), `src/ui/ui_screensaver.cpp` (bar),
`src/web_server.cpp` (`/api/stock-config`), `STOCK_UPDATE_MS` in
`src/config.h`.
