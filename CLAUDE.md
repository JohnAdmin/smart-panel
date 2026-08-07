# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Firmware for a WT32-SC01 Plus (ESP32-S3) smart-home touch panel: LVGL UI on a
480×320 ST7796, MQTT device control, a web config portal, scenes and schedules.

`docs/` is thorough and current — it owns the feature list, MQTT topic layout,
REST API table, partition map, system limits and factory-reset procedure.
Consult it rather than re-deriving any of that. `README.md` is now just an
overview plus a link table. This file covers what the docs do not: how to build
here, and the cross-cutting rules that are easy to violate because they live in
one file and bind everywhere else.

## Reading the docs

One file per topic, each 40–120 lines. **Read the one file you need — never the
whole set**, unless the task genuinely spans most of it (a rewrite, an audit).

| Need | File |
| --- | --- |
| Hardware, library versions, partition map, RGB565 colour rules | `docs/hardware.md` |
| Feature list (devices, scenes, schedules, screensaver, weather, portal, OTA) | `docs/features.md` |
| Dual-core split, data flow, storage, update frequencies, boot sequence, UI layer | `docs/architecture.md` |
| **MQTT topic structure** + payload formats + Homebridge compat | `docs/mqtt.md` |
| **Web API endpoint table**, editing the SPA | `docs/web-api.md` |
| Source file map, translation workflow | `docs/source-map.md` |
| System limits, default configuration | `docs/configuration.md` |
| Troubleshooting, factory reset, serial debug, known limitations | `docs/troubleshooting.md` |
| Stock ticker setup (diagnosis lives in the `stock-ticker` skill) | `docs/stock-ticker.md` |
| Changelog | `docs/changelog.md` |

Skip `docs/build.md` — this file supersedes it.

`docs/architecture.md` and `docs/hardware.md` restate the threading and display
rules below in prose; the authoritative short form is here. Keep both in sync
when either changes, and update this table when a doc is added or renamed.

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

Both `extra_scripts` are load-bearing, and both must stay *post* scripts:

- `fix_esptool.py` rewrites the uploader to `python -m esptool`, because Windows
  Defender quarantines `esptool.exe` out of the PlatformIO venv.
- `fix_otadata_offset.py` moves `boot_app0.bin` from the 0xE000 the Arduino
  builder hard-codes to the otadata offset `partitions_custom.csv` actually
  declares. 0xE000 sits inside this project's NVS partition, so without it every
  `upload` erased 8 KB of saved settings — silently, and only the most recently
  written keys. It patches `UPLOADERFLAGS` as well as `FLASH_EXTRA_IMAGES`;
  patching only the latter makes `envdump` look right while `upload` still
  erases NVS. **If the partition table moves, nothing here needs changing** —
  the offset is read from the CSV.

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

## Device types

`Device.dev_type` (`DEV_TOGGLE` / `DEV_DIMMER` / `DEV_FAN` / `DEV_AC`) decides
which control a tile gets. All three non-toggle types share **one numeric
channel** — `dimmer_topic` on the wire and `brightness` in memory. The names
and the `devices.json` key are historical and deliberately unchanged so old
configs load, but the value means brightness (0–100), fan speed (0–3) or target
°C (18–30) depending on the type; `Device::levelMin/levelMax/clampLevel` own
those ranges, and `set_device_level()` in `mqtt_manager.cpp` is the only writer.

A config with no `dev_type` key infers one — a device with a level topic
becomes a dimmer, everything else a switch — which is exactly how the panel
behaved before types existed. Both `device_store.cpp` and the `/api/save`
handler apply that same fallback; keep them in step.

## State and headers

`include/globals.h` declares **all** mutable global state and is the only place
new `extern` runtime state should go. `src/config.h` is compile-time constants
only — pins, limits, timeouts, defaults. Keep that separation.

## UI layer

`src/ui.cpp` builds the shell (nav rail, header, body container, toast);
`src/ui/*.cpp` build the screens. `src/ui/ui_screens.h` declares the
cross-screen surface.

`src/ui/ui_nav_rail.cpp` owns the left 52 px of any screen that calls
`ui_nav_rail_create()` — it is the top-level navigation (Home · Scenes ·
Sensors · Settings, plus a screensaver button). Scenes and schedules share one
destination with a pill tab row.
**Content on those screens is
laid out against `UI_CONTENT_W` / `UI_CONTENT_H`, not `SCREEN_WIDTH`.** Every
tile width in `ui_helpers.h` is sized so its column count survives that 428 px
area; changing one means re-checking the arithmetic in the comment above it.
Full-screen overlays (screensaver, dimmer modal) still use `SCREEN_WIDTH` —
they are meant to cover the rail.

`ui_ScreenMain` hosts **views**, not one fixed dashboard. Home (room cards),
a room's device tiles, favourites, the scene run-grid, the schedule list and
the sensors table all render into
`main_body_container`; `ui_show_main_view()` sets `s_view` and re-renders
through `rebuild_grid()`. Because every caller of `rebuild_grid()` (language
change, device edit, settings save) re-renders the *current* view, new views
need no extra wiring — but anything cached across a rebuild must be cleared at
the top of `rebuild_grid()` or it will point at freed objects.

Rooms are **discovered, not authored**: `room_sync_from_devices()` adds a
`Room` for each distinct `Device.room` string on every rebuild, and
`/rooms.json` only persists what a room adds on top (icon, climate topic). The
name is the join key — for `/api/rooms` too, which matches by name rather than
index because the list is rebuilt from devices on every boot.

A room's `climate_topic` is subscribed in `reconnect_mqtt()`, so **changing one
needs a restart** to take effect; the portal says so. Climate messages are only
matched after every device has failed to claim the topic, so a room can never
shadow a device.

Settings splits the content area again: a 116 px tab sidebar (`UI_SIDEBAR_W`,
built once by `build_settings_sidebar()`) and a `UI_SETTINGS_W` (312 px) body
that `build_settings_screen()` re-renders per tab. The keyboard is parented to
the *screen*, not to `set_container`, so it can span the full width over the
rail and sidebar — which is why `build_settings_screen()` deletes it explicitly
rather than relying on `lv_obj_clean()`.

**Never delete the object that is dispatching the event you are handling** —
LVGL keeps walking it after the callback returns. Hide it and rebuild later
(`ui_device_manager.cpp`'s inline rename), or defer the teardown with
`lv_async_call()` (the schedule delete, and any path that calls
`rebuild_grid()` from inside a widget's own callback).

Sub-screens (devices, scenes editor) are still real LVGL
screens and follow a `build_*` / `cleanup_*` pairing — sub-screens are destroyed to
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
  dimmest, so it gets crushed at surface brightness and what survives is R plus
  any excess G — so a ramp with G above R tilts visibly green. The G byte is
  the knob: if a trace of green shows, lower it.
- **`CLR_HEX_TEXT_LOW` is the contrast floor** — it carries 12 px labels and is
  tuned to clear 4.5:1 against the lightest background each of them can land on,
  including a translucent surface over a bright user wallpaper. Lowering it, or
  thinning a scrim underneath it, breaks that.

`lv_obj_set_style_transform_angle` is unusable on this target — it crashes
rather than degrading, and LVGL 8 has no 3D transform at all. Animate with
shadow, opacity or height instead; the flip clock in `ui_screensaver.cpp` shows
the pattern (height 0 → full plus a fade, 450 ms `lv_anim_path_ease_out`,
standing in for a card rotation).

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
