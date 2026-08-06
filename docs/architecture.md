# Architecture

[← Docs index](README.md)

## Dual-Core FreeRTOS

| Core | Task | Responsibility |
|------|------|----------------|
| Core 0 | network_task | WiFi, MQTT, OTA, weather API, web server |
| Core 1 | loop (UI) | LVGL rendering, touch input, screensaver, schedules |

Thread safety via `devices_mux` (device state) and `lvgl_mux` (UI rendering)
semaphores, both declared in `include/globals.h`.

The mutexes are **not recursive**, and every LVGL call made from Core 0 must
hold `lvgl_mux`. `src/mqtt_manager.cpp` is the model to copy: it takes the mutex
around each `ui_update_*` call with a timeout, and logs when it can't get it
rather than blocking. Code already running inside an LVGL event callback is on
Core 1 and must *not* take `lvgl_mux` itself — see the note at
`src/scene_manager.cpp:113`. `webActivityDetected` shows the pattern for async
web handlers: set a flag on Core 0, consume it in `loop()` on Core 1, never
touch LVGL from the handler.

## Data Flow

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

## Storage

| Storage | Content | Capacity |
|---------|---------|----------|
| NVS | WiFi/MQTT/UI settings (encrypted credentials) | 48KB |
| LittleFS | `/devices.json`, `/scenes.json`, `/schedules.json`, `/wallpaper.jpg` | 1.89MB |
| OTA | Dual app partitions for firmware rollback | 2×~3MB |

## Update Frequencies

| Component | Frequency | Core |
|-----------|-----------|------|
| LVGL Rendering | ~100Hz (10ms) | 1 |
| Time/Schedule Check | 1Hz | 1 |
| Weather Fetch | 30 min | 0 |
| MQTT Status Sync | 30s active, 5min screensaver | 0 |
| Device State Save | Max every 10s (debounced) | 0 |
| MQTT Heartbeat (sc01/status) | 30s (retained re-publish) | 0 |

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

## State and Headers

`include/globals.h` declares **all** mutable global state and is the only place
new `extern` runtime state should go. `src/config.h` is compile-time constants
only — pins, limits, timeouts, defaults. Keep that separation.

## UI Layer

`src/ui.cpp` builds the shell (header, body container, toast); `src/ui/*.cpp`
build the screens; `src/ui/ui_screens.h` declares the cross-screen surface.

Screens follow a `build_*` / `cleanup_*` pairing — sub-screens are destroyed to
reclaim LVGL heap before another is created. A new screen without its cleanup
will run the panel out of heap after enough navigation.

**Two colour systems coexist.** `ui_screens.h` holds the older theme-aware
macros (`CLR_BG_DEEP`, `CLR_TEXT_DIM`, …) that branch on `themeDark`;
`src/ui/ui_helpers.h` holds the current design-token ramp (`CLR_HEX_SURFACE_*`,
`CLR_HEX_TEXT_*`, `CLR_HEX_ACCENT*`) plus the shared widget factories. New work
should use the `ui_helpers.h` tokens. Colour choices are constrained by the
panel itself — see [Hardware → Display constraints](hardware.md#display-constraints).
