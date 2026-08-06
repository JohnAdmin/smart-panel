# SC01+ Smart Panel — Home Automation Touch Panel

ESP32-S3 based smart home control panel with 3.5" touchscreen, MQTT device control, web configuration, scene automation, and time-based scheduling.

| Aspect | Summary |
| --- | --- |
| Board | WT32-SC01 Plus (ESP32-S3, 480×320 ST7796, FT5x06 touch) |
| UI | LVGL 8.4 on LovyanGFX, dual-core FreeRTOS |
| Control | MQTT (Tasmota / HomeKit-Homebridge / Home Assistant payloads) |
| Config | Web portal at `smartpanel.local`, HTTP Basic Auth |
| Capacity | 100 devices · 8 scenes · 16 schedules |

## Quick start

```powershell
$env:PATH = "$env:USERPROFILE\.platformio\penv\Scripts;$env:PATH"
pio run -t upload            # firmware over USB
pio run -t uploadfs          # LittleFS image from data/
pio device monitor -b 115200 # serial log
```

Anything under `data/` needs `uploadfs`, not `upload`. Full build notes:
[docs/build.md](docs/build.md).

On first boot with no saved WiFi the panel comes up in AP mode as
`SC01-Plus-Setup`; join it and open `http://smartpanel.local` (`admin` /
`admin`) to configure WiFi and the MQTT broker.

## Documentation

| Doc | Covers |
| --- | --- |
| [Hardware & Platform](docs/hardware.md) | Board specs, library versions, partition layout, display constraints |
| [Features](docs/features.md) | Devices, offline queue, scenes, schedules, screensaver, weather, wallpaper, portal, OTA, settings |
| [Architecture](docs/architecture.md) | Dual-core split and mutexes, data flow, storage, update frequencies, boot sequence, UI layer |
| [MQTT](docs/mqtt.md) | Topic structure, panel topics, Homebridge compatibility, payload formats |
| [Web API & Portal](docs/web-api.md) | REST endpoint table, how to edit the SPA |
| [Source File Map](docs/source-map.md) | Which file does what, translation workflow |
| [Configuration & Limits](docs/configuration.md) | Max devices/scenes/schedules, shipped defaults |
| [Build & Flash](docs/build.md) | PlatformIO commands, `uploadfs` vs `upload`, OTA, fonts |
| [Troubleshooting](docs/troubleshooting.md) | Password reset, factory reset, serial debug, known limitations |
| [Stock Ticker](docs/stock-ticker.md) | Twelve Data setup, verification, diagnosis pointers |
| [Changelog](docs/changelog.md) | Dated feature notes |

Also in the repo root: [`STOCK_TICKER_RUNBOOK.md`](STOCK_TICKER_RUNBOOK.md)
(stock ticker diagnosis) and [`UPDATE_LOG.md`](UPDATE_LOG.md) (build history).

## Panel in trouble?

Forgot the portal password → Settings → System → **Reset Pass**. Panel stuck or
unreachable → hold a finger on the screen while powering on and keep holding 3
seconds for a factory reset. Both are spelled out in
[docs/troubleshooting.md](docs/troubleshooting.md).
