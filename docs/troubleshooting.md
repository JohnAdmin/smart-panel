# Troubleshooting & Factory Reset

[← Docs index](README.md)

## Reset Web Auth Password (from Panel)

If you forgot the web portal password but the panel UI is still accessible:

1. Open **Settings** (gear icon on home screen header)
2. Scroll down to the **System** card
3. Tap the red **"Reset Pass"** button
4. Web auth will be reset to **admin / admin**
5. Panel will reboot automatically

## Factory Reset

Two ways in, both wiping the same things (`factory_reset_wipe()` in
`src/main.cpp`): **Settings → System → Factory Reset** on the panel, which
confirms first, or the touch-and-hold-during-boot sequence below.
 (Touch & Hold during Boot)

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

After factory reset, the panel will boot in **AP mode**
(WiFi: `SC01-Plus-Setup`) for initial configuration.

### Cancel Factory Reset

If you accidentally triggered the reset screen, simply **release your finger**
before the progress bar fills. The panel will show "Cancelled" and boot
normally.

## Serial Debug

Connect USB-C and open a serial monitor at **115200 baud** to view boot logs,
MQTT activity, and error messages:

```bash
pio device monitor -b 115200
```

`[STOCK]` log lines have their own reference — see
[Stock ticker](stock-ticker.md#troubleshoot).

## Known Limitations

- **LVGL `transform_angle` not usable on ESP32** — `lv_obj_set_style_transform_angle`
  causes rendering failure and potential crash loops due to insufficient
  transform layer buffer on ESP32-S3. Use shadow/opacity-based animations
  instead.
- **No gradients on panel surfaces** — RGB565 without dithering quantises a
  near-neutral dark ramp to ~4 colours that read as green and purple seams.
  Depth comes from `bg_opa < 100%` instead. Details in
  [Hardware → Display constraints](hardware.md#display-constraints).
