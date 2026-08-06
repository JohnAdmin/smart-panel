---
name: stock-ticker
description: Diagnose and verify the Flip Clock screensaver stock ticker (Twelve Data API). Use when symbols show N/A, the stock bar is missing or stale after a config save, stock prices don't refresh, `[STOCK]` serial logs need interpreting, or after flashing a build that touches stock.cpp / ui_screensaver.cpp / the /api/stock-config endpoint. Also use before shipping any stock-related fix — it carries the required version-log step.
---

# Stock ticker runbook

Stock bar on the Flip Clock screensaver (**style 0 only**), fed by the Twelve Data
quote endpoint over plain HTTP.

For first-time setup (getting an API key, configuring symbols in the web portal)
read `docs/stock-ticker.md` instead — this skill covers diagnosis and
verification, not initial configuration.

## Where the feature lives

| Concern | Files |
| --- | --- |
| Fetch + parse | `src/stock.cpp`, `src/stock.h` |
| Polling / trigger | `src/wifi_manager.cpp` |
| Rendering | `src/ui/ui_screensaver.cpp`, `src/ui/ui_screens.h` |
| Config API + portal | `src/web_server.cpp` |
| Constants, globals | `src/config.h`, `include/globals.h` |

Fetching runs on **Core 0** (`network_task`). Any UI update from there must hold
`lvgl_mux` — see the threading section in `CLAUDE.md` before adding a path that
touches the stock bar.

## Read the display first

The two failure placeholders are not interchangeable — they narrow the cause
before you touch a log (`ui_screensaver.cpp:506–520`):

| On screen | Means |
| --- | --- |
| `SYM N/A` in grey | Symbol **is** configured; the API returned nothing usable for it |
| `--` | Slot is empty, or the label has not been populated yet |

So `--` on a slot you configured points at config not reaching the panel, while
`SYM N/A` points at the fetch or the symbol itself.

## Diagnose

Work the symptom that matches; the checks are ordered by likelihood, so stop at
the first one that explains the behaviour.

### All symbols show `N/A`

1. Wait 20–30 s after boot — the first fetch has real latency, `N/A` before it
   lands is expected, not a fault
2. Confirm WiFi is connected
3. Confirm the API key is present and its length looks right
4. Read the serial log for `[STOCK]` lines
5. Confirm the symbols are supported by the account's Twelve Data plan

### No stock bar at all

1. Screensaver style must be **0 (Flip Clock)** — the bar does not exist on
   other styles
2. Stock ticker enabled in the web config
3. Save the config again — the save is what invalidates the screensaver build
   cache, and a bar built while the feature was off stays stale until it does

### Only `AOT.BK` fails, others are fine

Expected, not a bug. The Twelve Data free tier does not reliably serve Thai SET
(`.BK`) symbols in this project's context. Typical: `code=404 symbol not found:
AOT.BK`, or a plan-related error depending on the account.

`AAPL` and `XAU/USD` keep working alongside it. Resolve by using symbols the
plan supports, upgrading the plan, or integrating a different API for SET data —
not by changing parsing code.

### Only one or two slots show data

Empty symbol slots render `--`. Either fill all three or accept the blanks —
there is nothing to fix unless a slot you configured is the blank one.

### Fetch stops after working for a while

Free-tier rate limit (800 calls/day, 8 req/min). The panel auto-retries on the
next 5-minute cycle; no action needed. If it never recovers, check the day quota
rather than the per-minute one.

### Log says the API key is invalid

Re-enter it in the portal. The field is masked (`****`) after the first save, so
a wrong key looks identical to a right one on screen — the log is the only place
that tells you.

## Serial log reference

Read from `src/stock.cpp`. Note the endpoint is plain **HTTP** — the switch off
HTTPS was the v2 fix, so an `https://` URL in a log means an old build.

```
[STOCK] Loaded: enabled=1  s0=AOT.BK  s1=PTT.BK  s2=AAPL
[STOCK] GET http://api.twelvedata.com/quote?symbol=AOT.BK,PTT.BK,AAPL&...
[STOCK] AAPL: 214.30 (+0.99%) market=open
[STOCK] AOT.BK: code=404 symbol not found
```

Failure lines worth recognising:

| Line | Meaning |
| --- | --- |
| `No API key, skipping fetch` | Never reached the network; key is unset |
| `http.begin failed` | URL or client setup, not the API |
| `HTTP <code>` | Transport reached the API; non-200 response |
| `<SYM>: code=<n> <msg>` | Per-symbol API rejection — the symbol, not the panel |
| `JSON parse error: <err>` | Response arrived but was not the expected shape |

## Verify

### After flash or reboot

Serial should carry `[STOCK] GET http://api.twelvedata.com/quote?...` followed by
valid lines for the supported symbols. Temporary `N/A` before that is expected.

### After saving config in the web portal

`POST /api/stock-config` must do three things — if any is missing, the symptom
above tells you which:

- persist the NVS values
- invalidate the screensaver build cache
- trigger an immediate fetch cycle

Then, on screen: layout updates **without** switching styles manually, valid
symbols show price and change, invalid symbols show grey `N/A`.

## Required step when fixing anything here

Append a version entry to
[references/version-history.md](references/version-history.md) — problem, root
cause, fix, result, key files. Read that file first when a bug looks familiar;
five prior fixes are logged there, including two (immediate-fetch-after-save,
rebuild invalidation) whose absence produces exactly the "stale bar" symptom
above.

Keep serial-log examples short and actionable, and keep the history aligned with
firmware behaviour on each release.
