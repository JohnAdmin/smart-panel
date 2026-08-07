"""
Puts boot_app0.bin at the otadata offset this project's partition table
actually declares, instead of the 0xE000 the Arduino builder hard-codes.

0xE000 is where otadata sits in the *stock* ESP32 layouts. This project moved
it: `partitions_custom.csv` gives NVS 32 KB at 0x9000, so NVS spans
0x9000-0x11000 and otadata starts at 0x11000. The hard-coded 0xE000 therefore
lands *inside* NVS, and every `pio run -t upload` printed

    Flash will be erased from 0x0000e000 to 0x0000ffff...

which wiped the last 8 KB — two of NVS's eight pages. NVS is log-structured, so
what disappeared was whatever had been written most recently: the panel kept its
Wi-Fi credentials from first boot but silently lost newly saved settings (the
weather city and its pinned coordinates) on every flash.

The builder derives this offset correctly for the ESP-IDF framework — see
`get_partition_info` in the platform's `frameworks/espidf.py` — but the Arduino
path appends a fixed `('0xe000', boot_app0.bin)` to FLASH_EXTRA_IMAGES. This
rewrites that entry from the CSV rather than changing the partition table, so
no stored data has to be sacrificed to fix it.

Two variables have to be corrected, not one. `builder/main.py` flattens
FLASH_EXTRA_IMAGES into UPLOADERFLAGS as `[offset, path]` pairs while the
platform script runs, so by the time any extra script executes, the offset the
uploader will actually use has already been copied out. Fixing only
FLASH_EXTRA_IMAGES makes `envdump` look right while `upload` still erases
0xE000.

Must be a POST script: both variables are populated while the framework is
processed, after pre-scripts run.
"""
import csv
import os

Import("env")  # noqa: F821 (provided by SCons / PlatformIO)


def _otadata_offset_from_csv(path):
    """Offset of the `data`/`ota` partition, or None if the table has none."""
    try:
        with open(path, newline="") as fh:
            for row in csv.reader(fh):
                cells = [c.strip() for c in row if c.strip()]
                if len(cells) < 4 or cells[0].startswith("#"):
                    continue
                # Name, Type, SubType, Offset, ...
                if cells[1] == "data" and cells[2] == "ota":
                    return int(cells[3], 0)
    except (OSError, ValueError) as exc:
        print(f"[fix_otadata] could not read {path}: {exc}")
    return None


csv_path = env.subst("$PARTITIONS_TABLE_CSV")
want = _otadata_offset_from_csv(csv_path) if csv_path else None

def _is_boot_app0(path):
    return "boot_app0" in os.path.basename(str(path)).lower()


def _wrong(offset):
    try:
        return int(str(offset), 0) != want
    except ValueError:
        return False


if want is None:
    print("[fix_otadata] no data/ota partition in the table — leaving offsets alone")
else:
    images = [
        (hex(want) if _is_boot_app0(image) and _wrong(offset) else offset, image)
        for offset, image in env.get("FLASH_EXTRA_IMAGES", [])
    ]
    env.Replace(FLASH_EXTRA_IMAGES=images)

    # `uploadfs` replaces UPLOADERFLAGS wholesale with a LittleFS-only command,
    # so there is nothing to patch there and the loop simply finds no match.
    flags = list(env.get("UPLOADERFLAGS", []))
    patched = 0
    for i, flag in enumerate(flags):
        if i and _is_boot_app0(flag) and _wrong(flags[i - 1]):
            print(f"[fix_otadata] boot_app0 {flags[i - 1]} -> {hex(want)} "
                  f"(otadata per {os.path.basename(csv_path)})")
            flags[i - 1] = hex(want)
            patched += 1
    if patched:
        env.Replace(UPLOADERFLAGS=flags)
