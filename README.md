# SC01+ Smart Panel — Home Automation Touch Panel

ESP32-S3 based smart home control panel with 3.5" touchscreen, MQTT device control, web configuration, scene automation, and time-based scheduling.

## Hardware

| Component | Spec |
|-----------|------|
| Board | WT32-SC01 Plus (ESP32-S3, dual-core 240MHz) |
| Display | 3.5" ST7796 LCD, 480×320, 8-bit parallel bus |
| Touch | FT5x06 capacitive (I2C, 400kHz) |
| Memory | 320KB SRAM, 8MB Flash, 8MB PSRAM |
| Connectivity | WiFi 802.11 b/g/n |
| Backlight | PWM (GPIO 45, 44.1kHz) |

## Platform & Libraries

| Library | Version | Purpose |
|---------|---------|---------|
| PlatformIO (pioarduino) | espressif32 @ 55.03.35 | Build system |
| Arduino Core | 3.3.5 (ESP-IDF 5.5.0) | Framework |
| LovyanGFX | 1.2.19 | LCD/touch driver |
| LVGL | 8.4.0 | UI framework |
| PubSubClient | 2.8.0 | MQTT client |
| ArduinoJson | 7.4.2 | JSON parsing |
| ESPAsyncWebServer | 3.6.0 | HTTP server |
| TJpg_Decoder | 1.1.0 | JPEG decoding |

## Features

### Device Control (MQTT)
- Up to **100 devices** with ON/OFF toggle and dimmer brightness (0–100%)
- 10 device icon types: Lamp, Fan, Switch, Plug, Thermostat, Lock, TV, Garage, Light Strip, Generic
- **Fan pulse glow animation** — breathing amber shadow effect (10→30px, 800ms cycle) when fan device is ON
- Room-based grouping with tabbed navigation
- Favorites system for quick access
- Supports Tasmota, HomeKit/HomeBridge, and Home Assistant MQTT payload formats
- Optimistic UI updates with rollback on publish failure
- 2-second debounce prevents rapid-toggle flicker
- 5-minute stale device detection

### Offline Queue
- 16-slot ring buffer queues MQTT commands when broker is unreachable
- Auto-replay on reconnect with exponential backoff (5s → 60s)
- Toast notifications for queued/replayed actions

### Scene Automation
- Up to **8 scenes**, each with up to **10 MQTT actions** (topic + payload)
- 6 predefined icons: Morning, Night, Leave, Movie, Party, Custom (color-coded)
- Execute from home dashboard tiles or web portal
- 50ms staggered publish between actions

### Time-Based Scheduling
- Up to **16 schedules** linked to scenes
- Hour/minute precision with day-of-week bitmask (Sun–Sat)
- Presets: Every day, Weekdays, Weekend
- Once-per-minute trigger gate prevents duplicate fires

### Screensaver
- **Flip Clock** — Premium glass-morphism panels (HH:MM), warm amber accents, pulsing animated colon, weather + color-coded WiFi/MQTT status bar, panel title display
- **Minimal** — Clean centered HH:MM with amber accent underline, date, weather, and status
- **Screen Off** — Pure black, wake-on-touch
- Configurable idle timeout (default 2 minutes)
- Auto-dim to 80/255 brightness, restore on wake
- Re-subscribes MQTT on wake for state sync

### Weather
- Open-Meteo API (free, no key required)
- City name geocoding → lat/lon → current temperature + description
- WMO weather code mapping (Clear, Cloudy, Rain, Snow, etc.)
- 30-minute update interval
- Fallback: Bangkok, Thailand

### Wallpaper
- Custom JPEG/PNG/BMP upload via web portal
- Auto-detect format by file magic bytes (not extension)
- Decoded to 480×320 RGB565 in PSRAM
- 3 built-in default presets

### Web Configuration Portal
- Single Page App with Tailwind CSS dark theme (glass-morphism design)
- HTTP Basic Auth (default admin/admin)
- mDNS: `smartpanel.local`

### OTA Updates
- ArduinoOTA + web portal firmware upload (`/api/fw/upload`)
- Dual OTA partitions (app0/app1) for safe rollback

### Settings
- WiFi SSID/password, MQTT broker/credentials
- NTP timezone (GMT±12), weather city
- Panel title, theme (dark/light), grid layout (normal/large)
- Time format (12h/24h), display brightness (0–255)
- Screensaver style and timeout
- Web portal auth credentials
- Credentials encrypted with MAC-based XOR + Base64 in NVS

## Architecture

### Dual-Core FreeRTOS

| Core | Task | Responsibility |
|------|------|----------------|
| Core 0 | network_task | WiFi, MQTT, OTA, weather API, web server |
| Core 1 | loop (UI) | LVGL rendering, touch input, screensaver, schedules |

Thread safety via `devices_mux` (device state) and `lvgl_mux` (UI rendering) semaphores.

### Data Flow

```
User Touch / Web Portal
        ↓
  MQTT Publish (cmnd_topic)  ←→  MQTT Broker  ←→  Smart Devices
        ↓
  State Update (state_topic, RETAIN)
        ↓
  Device Array + UI Refresh
        ↓
  Persist to LittleFS (debounced 10s)
```

### Storage

| Storage | Content | Capacity |
|---------|---------|----------|
| NVS | WiFi/MQTT/UI settings (encrypted credentials) | 48KB |
| LittleFS | `/devices.json`, `/scenes.json`, `/schedules.json`, `/wallpaper.jpg` | 1.89MB |
| OTA | Dual app partitions for firmware rollback | 2×~3MB |

### Update Frequencies

| Component | Frequency | Core |
|-----------|-----------|------|
| LVGL Rendering | ~100Hz (10ms) | 1 |
| Time/Schedule Check | 1Hz | 1 |
| Weather Fetch | 30 min | 0 |
| MQTT Status Sync | 30s active, 5min screensaver | 0 |
| Device State Save | Max every 10s (debounced) | 0 |
| MQTT Heartbeat (sc01/status) | 30s (retained re-publish) | 0 |

## MQTT Topic Structure

### Per-Device Topics
- **State** (subscribe): `home/<device>/stat/POWER` — current ON/OFF (retained)
- **Command** (publish): `home/<device>/cmnd/POWER` — toggle command
- **Dimmer** (publish): `home/<device>/cmnd/Dimmer` — brightness 0–100

### Panel Topics
- `sc01/status` — Panel heartbeat: `online` / `offline` (LWT, retained), re-published every 30s
- `homebridge/#` — Wildcard for HomeKit bridge compatibility

### Homebridge MQTTThing Compatibility
- Device payloads use `ON`/`OFF` matching MQTTThing `onValue`/`offValue`
- Panel status exposed as **switch** type using `sc01/status` topic (`onValue: "online"`, `offValue: "offline"`)
- Retained heartbeat every 30s keeps Homebridge status in sync

### Payload Formats
Supports: `ON`/`OFF`, `1`/`0`, `true`/`false`, JSON `{"POWER":"ON"}`, `{"state":true}`, `{"Status":{"Power":1}}`

## Web API Endpoints

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
| POST | `/api/fw/upload` | OTA firmware binary upload |
| POST | `/api/wallpaper/upload` | Custom wallpaper upload |
| GET | `/api/wallpaper/status` | Check wallpaper existence |
| GET | `/api/qrcode` | WiFi QR code SVG |
| GET | `/default[1-3].jpg` | Built-in preset wallpapers |

## Source File Map

```
main.cpp                  System init, dual-core tasks, LVGL loop, screensaver
├── config.h              Hardware pins, constants, limits, defaults
├── globals.h             All extern runtime state declarations
├── device.h              Device struct (name, room, topics, state, brightness)
├── hal.cpp/h             LovyanGFX LCD/touch drivers, LVGL callbacks
├── device_store.cpp      Device persistence (JSON + legacy binary migration)
├── wifi_manager.cpp/h    WiFi, NTP, OTA, NVS settings, encryption
├── mqtt_manager.cpp/h    MQTT broker, offline queue, state sync, toggle
├── web_server.cpp/h      HTTP portal, REST API, SPA frontend
├── weather.cpp           Open-Meteo API (geocoding + forecast)
├── scene_manager.cpp     Scene persistence + execution
├── schedule_manager.cpp  Schedule persistence + time-based triggers
├── wallpaper_helper.h    Wallpaper decode forward declaration
└── ui.cpp                LVGL screen init, header, grid, toast
    ├── ui/ui_helpers.h          Shared UI factories (pill btn, glass card, etc.)
    ├── ui/ui_screens.h          Screen/callback declarations
    ├── ui/ui_main_screen.cpp    Dashboard, device tiles, toggle events
    ├── ui/ui_settings.cpp       Settings form (WiFi, MQTT, theme, system)
    ├── ui/ui_device_manager.cpp Device CRUD (list, add, edit, delete)
    ├── ui/ui_dimmer_modal.cpp/h Brightness slider modal
    ├── ui/ui_screensaver.cpp    Flip clock, minimal, off modes
    ├── ui/ui_wallpaper.cpp/h    Background image decode/render
    ├── ui/ui_scene_manager.cpp  Scene list/edit/execute UI
    └── ui/ui_schedule.cpp       Schedule display + enable/disable toggle
```

## System Limits

| Resource | Limit |
|----------|-------|
| Devices | 100 |
| Scenes | 8 |
| Schedules | 16 |
| Actions per Scene | 10 |
| MQTT Offline Queue | 16 commands |
| MQTT Buffer | 512 bytes |
| Device Name / Room | 24 chars |
| Topic Strings | 64 chars |
| Scene Name | 24 chars |

## Partition Layout

| Name | Type | Offset | Size |
|------|------|--------|------|
| nvs | data | 0x9000 | 32KB |
| otadata | data | 0x11000 | 8KB |
| app0 | OTA_0 | 0x20000 | 2.94MB |
| app1 | OTA_1 | 0x310000 | 3MB |
| spiffs | LittleFS | 0x610000 | 1.94MB |

## Build

```bash
# Build firmware
pio run

# Upload firmware
pio run --target upload

# Upload filesystem (LittleFS)
pio run --target uploadfs

# Monitor serial
pio device monitor -b 115200
```

## Boot Sequence

1. GPIO/UART init, disable WDT
2. Create mutexes (`lvgl_mux`, `devices_mux`)
3. Load persistence: NVS settings → devices.json → scenes.json → schedules.json
4. HAL init (LCD + touch via LovyanGFX)
5. LVGL init (draw buffers, driver registration)
6. UI init (build all screens, scene/schedule tiles)
7. Network setup: WiFi → NTP → mDNS → ArduinoOTA → web server → MQTT
8. Re-enable WDT (5s timeout)
9. Launch `network_task` on Core 0
10. Enter UI loop on Core 1

## Default Configuration

| Setting | Default |
|---------|---------|
| WiFi | SC01-Plus-Setup (AP mode if no config) |
| MQTT Port | 1883 |
| mDNS | smartpanel.local |
| Web Auth | admin / admin |
| Theme | Dark |
| Grid Layout | Normal (3-column) |
| Time Format | 24-hour |
| Timezone | GMT+7 |
| Weather City | Bangkok |
| Screensaver | Flip Clock, 2 min timeout |
| Brightness | 255 (max) |

## Troubleshooting & Factory Reset

### Reset Web Auth Password (from Panel)
If you forgot the web portal password but the panel UI is still accessible:
1. Open **Settings** (gear icon on home screen header)
2. Scroll down to the **System** card
3. Tap the red **"Reset Pass"** button
4. Web auth will be reset to **admin / admin**
5. Panel will reboot automatically

### Factory Reset (Touch & Hold during Boot)
If the system is unresponsive, stuck, or you cannot access any screen:
1. **Power off** the SC01+ panel (unplug USB-C)
2. **Touch and hold** your finger on the screen
3. While still holding, **plug in USB-C** to power on
4. The screen will show: **"Touch & hold 3s to Factory Reset"**
5. **Keep holding** — a red progress bar will fill over 3 seconds
6. When the bar is full, all data will be erased and the panel reboots

**What gets erased:**
- WiFi SSID & password
- MQTT broker settings & credentials
- Web portal username & password (reset to admin/admin)
- All device configurations (`/devices.json`)
- All scenes (`/scenes.json`)
- All schedules (`/schedules.json`)
- Custom wallpaper (`/wallpaper.jpg`)
- Theme, brightness, timezone, and all other settings

**What is preserved:**
- Firmware (the program itself stays intact)
- LittleFS filesystem structure

After factory reset, the panel will boot in **AP mode** (WiFi: `SC01-Plus-Setup`) for initial configuration.

## Known Limitations

- **LVGL `transform_angle` not usable on ESP32** — `lv_obj_set_style_transform_angle` causes rendering failure and potential crash loops due to insufficient transform layer buffer on ESP32-S3. Use shadow/opacity-based animations instead.

### Cancel Factory Reset
If you accidentally triggered the reset screen, simply **release your finger** before the progress bar fills. The panel will show "Cancelled" and boot normally.

### Serial Debug
Connect USB-C and open serial monitor at **115200 baud** to view boot logs, MQTT activity, and error messages:
```bash
pio device monitor -b 115200
```

---

## Stock Ticker — Setup & Testing

### 1. Get a Twelve Data API Key (Free)

1. Go to [twelvedata.com](https://twelvedata.com) → Sign Up (free)
2. Dashboard → **API Keys** → copy the key
3. Free tier: **800 API calls/day**, **8 req/min** — more than enough (panel fetches every 5 minutes)

### 2. Configure via Web Portal

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

### 3. Verify on Screensaver

1. Let the panel idle for 2 minutes (or reduce screensaver timeout in Settings)
2. The **Flip Clock** screensaver will show a compact stock bar above the status strip
3. Each slot displays: `SYM  price  ±x.xx%` — green for up, red for down
4. If market is closed, a ` z` suffix is appended (e.g. `AOT  45.25  +1.20% z`)
5. Labels update every 5 minutes while WiFi is connected

### 4. Troubleshoot

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| Labels show `--` | Stock fetch failed or not yet fetched | Check serial log for `[STOCK]` lines; verify API key |
| `"Invalid API key"` in log | Wrong key | Re-enter key in portal; key is masked (`****`) after first save |
| No bar visible | Enable toggle is OFF | Go to Stock Ticker tab → enable |
| Only 1–2 symbols show | Empty symbol slots | Fill all 3 slots or leave blank intentionally |
| Fetch stops after a while | Rate limit (free tier 8 req/min) | Normal — panel auto-retries after 5 min |

### 5. Serial Log Reference

```
[STOCK] Fetching: https://api.twelvedata.com/quote?symbol=AOT.BK,PTT.BK,AAPL&...
[STOCK] AOT.BK  price=45.25  chg=+0.50  pct=+1.12%  open=1
[STOCK] PTT.BK  price=32.00  chg=-0.25  pct=-0.77%  open=1
[STOCK] AAPL    price=214.30 chg=+2.10  pct=+0.99%  open=0
```

---

## Stock Ticker Runbook

- See `STOCK_TICKER_RUNBOOK.md` for full incident history, root causes, fixes, verification steps, and version-by-version tracking.

## Changelog

### 2026-06-03 — Stock Ticker (Twelve Data)

**feat: add stock ticker (Twelve Data) on flip-clock screensaver**

- `src/stock.h` + `src/stock.cpp` — fetch 3 configurable symbols via Twelve Data API (batch quote endpoint, 5-min interval)
- `src/config.h` — added `STOCK_UPDATE_MS 300000` constant
- `include/globals.h` — `#include "../src/stock.h"` for universal access
- `src/wifi_manager.cpp` — `loadStockConfig()` on boot; immediate fetch on WiFi connect; 5-min refresh loop
- `src/ui/ui_screensaver.cpp` — compact 3-label stock bar above status bar in Flip Clock (Style 0) screensaver
- `src/web_server.cpp` — **Stock Ticker** settings tab: enable toggle, 3 symbol inputs (supports Thai SET `.BK` suffix and US stocks), API key with mask; `/api/stock-config` GET + POST endpoints (NVS-backed)
- `platformio.ini` — restored `extra_scripts = post:fix_esptool.py` (was corrupted)
