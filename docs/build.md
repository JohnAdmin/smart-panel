# Build & Flash

[← Docs index](README.md)

`pio` is not on the Bash `PATH` on the primary dev machine. Use PowerShell and
prepend the PlatformIO venv:

```powershell
$env:PATH = "$env:USERPROFILE\.platformio\penv\Scripts;$env:PATH"
pio run                      # build
pio run -t upload            # flash firmware over USB (upload_port = COM3)
pio run -t uploadfs          # flash LittleFS image from data/
pio device monitor -b 115200 # serial log
```

A full build is ~40–90 s. There are no tests — `test/` holds only the stock
PlatformIO README.

## Filesystem vs firmware

**Anything under `data/` needs `uploadfs`, not `upload`.** That covers
`wallpaper.jpg`, `devices.json` and the translation files; a plain firmware
flash leaves the old copies in place.

## OTA instead of USB

`platformio.ini` carries a commented `espota` block — switch `upload_port` to
the panel IP and set `upload_protocol = espota`. The web portal can also take a
firmware binary at `/api/fw/upload`.

## fix_esptool.py

`extra_scripts = post:fix_esptool.py` is load-bearing: it rewrites the uploader
to `python -m esptool` because Windows Defender quarantines `esptool.exe` out of
the PlatformIO venv. It must stay a *post* script.

## Fonts

`regen_fonts.ps1` regenerates the `lv_font_montserrat_*.c` files via
`lv_font_conv`, merging Montserrat + Sarabun (Thai, `0x0E00-0x0E7F`) + a
hand-picked FontAwesome subset. Run it only when a glyph range changes; the
generated `.c` files in `src/` are large and not worth reading.
