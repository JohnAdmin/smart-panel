# Features

[← Docs index](README.md)

## Device Control (MQTT)

- Up to **100 devices** with ON/OFF toggle and dimmer brightness (0–100%)
- 10 device icon types: Lamp, Fan, Switch, Plug, Thermostat, Lock, TV, Garage, Light Strip, Generic
- **Fan pulse glow animation** — breathing amber shadow effect (10→30px, 800ms cycle) when fan device is ON
- Room-based grouping with tabbed navigation
- Favorites system for quick access
- Supports Tasmota, HomeKit/HomeBridge, and Home Assistant MQTT payload formats
- Optimistic UI updates with rollback on publish failure
- 2-second debounce prevents rapid-toggle flicker
- 5-minute stale device detection

Topic layout and payload formats: [MQTT](mqtt.md).

## Offline Queue

- 16-slot ring buffer queues MQTT commands when broker is unreachable
- Auto-replay on reconnect with exponential backoff (5s → 60s)
- Toast notifications for queued/replayed actions

## Scene Automation

- Up to **8 scenes**, each with up to **10 MQTT actions** (topic + payload)
- 6 predefined icons: Morning, Night, Leave, Movie, Party, Custom (color-coded)
- Execute from home dashboard tiles or web portal
- 50ms staggered publish between actions

## Time-Based Scheduling

- Up to **16 schedules** linked to scenes
- Hour/minute precision with day-of-week bitmask (Sun–Sat)
- Presets: Every day, Weekdays, Weekend
- Once-per-minute trigger gate prevents duplicate fires

## Screensaver

- **Flip Clock** — Premium glass-morphism panels (HH:MM), warm amber accents, pulsing animated colon, weather + color-coded WiFi/MQTT status bar, panel title display
- **Minimal** — Clean centered HH:MM with amber accent underline, date, weather, and status
- **Screen Off** — Pure black, wake-on-touch
- Configurable idle timeout (default 2 minutes)
- Auto-dim to 80/255 brightness, restore on wake
- Re-subscribes MQTT on wake for state sync

The Flip Clock style also carries the stock ticker bar — see
[Stock ticker](stock-ticker.md).

## Weather

- Open-Meteo API (free, no key required)
- City name geocoding → lat/lon → current temperature + description
- WMO weather code mapping (Clear, Cloudy, Rain, Snow, etc.)
- 30-minute update interval
- Fallback: Bangkok, Thailand

## Wallpaper

- Custom JPEG/PNG/BMP upload via web portal
- Auto-detect format by file magic bytes (not extension)
- Decoded to 480×320 RGB565 in PSRAM
- 3 built-in default presets

## Web Configuration Portal

- Single Page App with Tailwind CSS dark theme (glass-morphism design)
- HTTP Basic Auth (default admin/admin)
- mDNS: `smartpanel.local`

Endpoint table: [Web API](web-api.md).

## OTA Updates

- ArduinoOTA + web portal firmware upload (`/api/fw/upload`)
- Dual OTA partitions (app0/app1) for safe rollback

## Settings

- WiFi SSID/password, MQTT broker/credentials
- NTP timezone (GMT±12), weather city
- Panel title, theme (dark/light), grid layout (normal/large)
- Time format (12h/24h), display brightness (0–255)
- Screensaver style and timeout
- Web portal auth credentials
- Credentials encrypted with MAC-based XOR + Base64 in NVS

Shipped defaults: [Configuration](configuration.md).
