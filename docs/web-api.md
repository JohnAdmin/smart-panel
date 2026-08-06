# Web API & Portal

[← Docs index](README.md)

Reachable at `http://smartpanel.local` (mDNS) or the panel's IP. HTTP Basic
Auth, default `admin` / `admin`.

## Endpoints

| Method | Endpoint | Purpose |
|--------|----------|---------|
| GET/PUT | `/api/config` | WiFi, MQTT, timezone, theme settings |
| GET/POST | `/api/devices` | Device list + bulk import |
| PUT/DELETE | `/api/devices/:id` | Edit/delete device |
| GET/POST | `/api/scenes` | Scene list + create |
| PUT/DELETE | `/api/scenes/:id` | Edit/delete scene |
| GET/POST | `/api/schedules` | Schedule list + create |
| PUT/DELETE | `/api/schedules/:id` | Edit/delete schedule |
| GET | `/api/status` | Real-time device states |
| GET/POST | `/api/stock-config` | Stock ticker symbols + API key (NVS-backed) |
| POST | `/api/fw/upload` | OTA firmware binary upload |
| POST | `/api/wallpaper/upload` | Custom wallpaper upload |
| GET | `/api/wallpaper/status` | Check wallpaper existence |
| GET | `/api/qrcode` | WiFi QR code SVG |
| GET | `/default[1-3].jpg` | Built-in preset wallpapers |

## Editing the Portal

`src/web_page.cpp` is the entire single-page app as one `PROGMEM` string
literal, ~2000 lines. Editing the portal means editing that string.
`src/web_server.cpp` serves it and implements the REST API above.

`src/patch_ui_and_web.py` is a spent one-off migration script, not part of the
build.

Async handlers run on Core 0 and must never touch LVGL directly — see
[Architecture → Dual-core](architecture.md#dual-core-freertos).
