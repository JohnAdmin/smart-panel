# Source File Map

[← Docs index](README.md)

```
main.cpp                  System init, dual-core tasks, LVGL loop, screensaver
├── config.h              Hardware pins, constants, limits, defaults
├── globals.h             All extern runtime state declarations
├── device.h              Device struct (name, room, topics, state, brightness)
├── hal.cpp/h             LovyanGFX LCD/touch drivers, LVGL callbacks
├── device_store.cpp      Device persistence (JSON + legacy binary migration)
├── wifi_manager.cpp/h    WiFi, NTP, OTA, NVS settings, encryption
├── mqtt_manager.cpp/h    MQTT broker, offline queue, state sync, toggle
├── web_server.cpp/h      HTTP portal, REST API
├── web_page.cpp          SPA frontend as a single PROGMEM string
├── weather.cpp           Open-Meteo API (geocoding + forecast)
├── stock.cpp/h           Twelve Data quotes for the screensaver ticker
├── scene_manager.cpp     Scene persistence + execution
├── schedule_manager.cpp  Schedule persistence + time-based triggers
├── lang.cpp/h            Runtime translations, L(LangKey) lookup
├── wallpaper_helper.h    Wallpaper decode forward declaration
└── ui.cpp                LVGL screen init, header, grid, toast
    ├── ui/ui_helpers.h          Shared UI factories + design tokens
    ├── ui/ui_screens.h          Screen/callback declarations, legacy theme macros
    ├── ui/ui_main_screen.cpp    Dashboard, device tiles, toggle events
    ├── ui/ui_settings.cpp       Settings form (WiFi, MQTT, theme, system)
    ├── ui/ui_device_manager.cpp Device CRUD (list, add, edit, delete)
    ├── ui/ui_dimmer_modal.cpp/h Brightness slider modal
    ├── ui/ui_screensaver.cpp    Flip clock, minimal, off modes + stock bar
    ├── ui/ui_wallpaper.cpp/h    Background image decode/render
    ├── ui/ui_scene_manager.cpp  Scene list/edit/execute UI
    └── ui/ui_schedule.cpp       Schedule display + enable/disable toggle
```

Generated `lv_font_montserrat_*.c` files also live in `src/` — they are produced
by `regen_fonts.ps1` and not worth reading. See [Build](build.md#fonts).

## Translations

`L(LangKey)` resolves a string. English is compiled in as `defaults[]`; other
languages override at runtime from `/lang_<code>.json` in LittleFS, matched by
the names in `key_names[]`.

Adding a string means four edits kept in sync:

1. the `LangKey` enum in `src/lang.h`
2. `defaults[]` in `src/lang.cpp`
3. `key_names[]` in `src/lang.cpp`
4. the JSON files under `data/`

A missing JSON key silently falls back to English rather than erroring. Changed
files under `data/` need `pio run -t uploadfs`, not a plain firmware flash.
