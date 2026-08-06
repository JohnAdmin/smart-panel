# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Firmware for a WT32-SC01 Plus (ESP32-S3) smart-home touch panel: LVGL UI on a
480×320 ST7796, MQTT device control, a web config portal, scenes and schedules.

`README.md` is thorough and current — it owns the feature list, MQTT topic
layout, REST API table, partition map, system limits and factory-reset
procedure. Read it rather than re-deriving any of that. This file covers what
the README does not: how to build here, and the cross-cutting rules that are
easy to violate because they live in one file and bind everywhere else.

## Build & flash

`pio` is not on the Bash `PATH` on this machine. Use PowerShell and prepend the
PlatformIO venv:

```powershell
$env:PATH = "$env:USERPROFILE\.platformio\penv\Scripts;$env:PATH"
pio run                      # build
pio run -t upload            # flash firmware over USB (upload_port = COM3)
pio run -t uploadfs          # flash LittleFS image from data/
pio device monitor -b 115200 # serial log
```

A full build is ~40–90 s. There are no tests — `test/` holds only the stock
PlatformIO README.

**Anything under `data/` needs `uploadfs`, not `upload`.** That covers
`wallpaper.jpg`, `devices.json` and the translation files; a plain firmware
flash leaves the old copies in place.

For OTA instead of USB, `platformio.ini` carries a commented `espota` block —
switch `upload_port` to the panel IP and `upload_protocol = espota`.

`extra_scripts = post:fix_esptool.py` is load-bearing: it rewrites the uploader
to `python -m esptool` because Windows Defender quarantines `esptool.exe` out of
the PlatformIO venv. It must stay a *post* script.

`regen_fonts.ps1` regenerates the `lv_font_montserrat_*.c` files via
`lv_font_conv`, merging Montserrat + Sarabun (Thai, `0x0E00-0x0E7F`) + a
hand-picked FontAwesome subset. Run it only when a glyph range changes; the
generated `.c` files in `src/` are large and not worth reading.

## Threading

Two cores, and the split is the source of most subtle bugs:

- **Core 0 — `network_task`**: WiFi, MQTT, OTA, weather, stocks, web server.
- **Core 1 — `loop()`**: `lv_timer_handler`, touch, screensaver, schedules.

Two mutexes in `globals.h` guard the boundary:

- `lvgl_mux` — **every LVGL call made from Core 0 must hold it.** `mqtt_manager.cpp`
  is the model: it takes the mutex around each `ui_update_*` call, with a
  timeout, and logs when it can't get it rather than blocking.
- `devices_mux` — protects `devices[]` and `mqttClient`, which both cores touch.

The mutexes are **not recursive**. Code already running inside an LVGL event
callback is on Core 1 and holds nothing, but must not take `lvgl_mux` itself —
see the note at `scene_manager.cpp:113`. When adding a path that updates the UI,
first work out which core it runs on; getting this wrong deadlocks or corrupts
the display rather than failing loudly.

`webActivityDetected` shows the intended pattern for async web handlers: set a
flag on Core 0, consume it in `loop()` on Core 1, never touch LVGL from the
handler.

## State and headers

`include/globals.h` declares **all** mutable global state and is the only place
new `extern` runtime state should go. `src/config.h` is compile-time constants
only — pins, limits, timeouts, defaults. Keep that separation.

## UI layer

`src/ui.cpp` builds the shell (header, body container, toast); `src/ui/*.cpp`
build the screens. `src/ui/ui_screens.h` declares the cross-screen surface.

Screens follow a `build_*` / `cleanup_*` pairing — sub-screens are destroyed to
reclaim LVGL heap before another is created. If you add a screen, add its
cleanup and call it, or the panel will run out of heap after enough navigation.

**Two colour systems coexist.** `ui_screens.h` holds the older theme-aware
macros (`CLR_BG_DEEP`, `CLR_TEXT_DIM`, …) that branch on `themeDark`.
`src/ui/ui_helpers.h` holds the current design-token ramp (`CLR_HEX_SURFACE_*`,
`CLR_HEX_TEXT_*`, `CLR_HEX_ACCENT*`) plus the shared widget factories. New work
should use the `ui_helpers.h` tokens; the old macros survive on screens that
have not been migrated.

### Display constraints that shape the design

The panel is RGB565 with `LV_DITHER_GRADIENT` off, and that is not a free
parameter — the header comment above `CLR_HEX_SURFACE_0` in `ui_helpers.h`
records the measurements behind three rules. Read it before touching surface or
text colours:

- **No gradients on surfaces.** A ramp between two near-neutral darks quantises
  to ~4 colours whose transitions each move a single channel, so they render as
  green and purple seams. Retuning endpoints cannot help (LVGL quantises the
  stops before interpolating) and enabling dithering is worse (fixed ±32
  amplitude, wider than the ramp). Depth comes from `bg_opa < 100 %` letting the
  wallpaper read through instead.
- **Keep R = G** in surface and text colours. The panel's blue subpixel is
  dimmest, so any R > G tilts visibly green at low brightness.
- **`CLR_HEX_TEXT_LOW` is the contrast floor** — it carries 12 px labels and is
  tuned to clear 4.5:1 against the lightest background each of them can land on,
  including a translucent surface over a bright user wallpaper. Lowering it, or
  thinning a scrim underneath it, breaks that.

`lv_obj_set_style_transform_angle` is unusable on this target — it crashes
rather than degrading. Animate with shadow/opacity instead.

## Web portal

`src/web_page.cpp` is the entire single-page app as one `PROGMEM` string
literal, ~2000 lines; `src/web_server.cpp` serves it and implements the REST
API. Editing the portal means editing that string. `src/patch_ui_and_web.py` is
a spent one-off migration script, not part of the build.

## Translations

`L(LangKey)` resolves a string. English is compiled in as `defaults[]`; other
languages override at runtime from `/lang_<code>.json` in LittleFS, matched by
the names in `key_names[]`. Adding a string means four edits kept in sync: the
`LangKey` enum in `lang.h`, and `defaults[]` plus `key_names[]` in `lang.cpp`,
then the JSON files under `data/`. A missing JSON key silently falls back to
English rather than erroring.
