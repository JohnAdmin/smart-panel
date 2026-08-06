"""
Permanent workaround: invoke esptool as a Python module instead of
esptool.exe, which Windows Defender frequently quarantines as a false
positive in %USERPROFILE%\\.platformio\\penv\\Scripts\\.

Must be loaded as a POST script (extra_scripts = post:fix_esptool.py)
because PlatformIO's main.py sets ERASETOOL / OBJCOPY / UPLOADER to
esptool.exe AFTER pre-scripts run.
"""
import sys
import importlib.util

Import("env")  # noqa: F821 (provided by SCons / PlatformIO)

PYTHON = env.subst("$PYTHONEXE") or sys.executable
ESPTOOL_CMD = f'"{PYTHON}" -m esptool'

# Surface a clear error early if the module isn't installed, instead of
# letting the build fail later with a cryptic message.
if importlib.util.find_spec("esptool") is None:
    print(f'[fix_esptool] ERROR: esptool module missing. Run: "{PYTHON}" -m pip install esptool')

# Force every esptool-using variable to the Python-module form. We
# always replace (not "only when esptool.exe is missing") so future
# Defender removals don't break the build.
for var in ("ERASETOOL", "OBJCOPY", "UPLOADER"):
    current = env.get(var, "")
    if current and "esptool" in str(current).lower():
        env.Replace(**{var: ESPTOOL_CMD})
        print(f"[fix_esptool] {var} -> {ESPTOOL_CMD}")


# ---------------------------------------------------------------------
# Custom target: `uploadapp` — flash firmware to BOTH OTA app slots
# (0x20000 and 0x310000).
# Skips bootloader, partitions, and NVS so saved WiFi credentials,
# language, and other Preferences survive across re-flashes.
#
# Usage:  pio run -e wt32-sc01-plus -t uploadapp
# ---------------------------------------------------------------------
def _upload_app_only(target, source, env):
    firmware = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    port = env.subst("$UPLOAD_PORT") or "COM4"
    speed = env.subst("$UPLOAD_SPEED") or "921600"
    cmd = (
        f'"{PYTHON}" -m esptool --chip esp32s3 --port {port} '
        f'--baud {speed} --before default_reset --after hard_reset '
        f'write_flash -z '
        f'0x20000 "{firmware}" '
        f'0x310000 "{firmware}"'
    )
    print(f"[uploadapp] {cmd}")
    return env.Execute(cmd)


env.AddCustomTarget(
    name="uploadapp",
    dependencies="$BUILD_DIR/${PROGNAME}.bin",
    actions=[_upload_app_only],
    title="Upload app only",
    description="Flash firmware (0x20000 + 0x310000) without touching bootloader/partitions/NVS.",
)
