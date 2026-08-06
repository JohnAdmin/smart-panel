# Hardware & Platform

[← Docs index](README.md)

## Hardware

| Component | Spec |
|-----------|------|
| Board | WT32-SC01 Plus (ESP32-S3, dual-core 240MHz) |
| Display | 3.5" ST7796 LCD, 480×320, 8-bit parallel bus |
| Touch | FT5x06 capacitive (I2C, 400kHz) |
| Memory | 320KB SRAM, 8MB Flash, 8MB PSRAM |
| Connectivity | WiFi 802.11 b/g/n |
| Backlight | PWM (GPIO 45, 44.1kHz) |

Hardware pin assignments live in `src/config.h`.

## Platform & Libraries

| Library | Version | Purpose |
|---------|---------|---------|
| PlatformIO (pioarduino) | espressif32 @ 55.03.35 | Build system |
| Arduino Core | 3.3.5 (ESP-IDF 5.5.0) | Framework |
| LovyanGFX | 1.2.19 | LCD/touch driver |
| LVGL | 8.4.0 | UI framework |
| PubSubClient | 2.8.0 | MQTT client |
| ArduinoJson | 7.4.2 | JSON parsing |
| ESPAsyncWebServer | 3.6.0 | HTTP server |
| TJpg_Decoder | 1.1.0 | JPEG decoding |

## Partition Layout

Defined in `partitions_custom.csv`.

| Name | Type | Offset | Size |
|------|------|--------|------|
| nvs | data | 0x9000 | 32KB |
| otadata | data | 0x11000 | 8KB |
| app0 | OTA_0 | 0x20000 | 2.94MB |
| app1 | OTA_1 | 0x310000 | 3MB |
| spiffs | LittleFS | 0x610000 | 1.94MB |

## Display Constraints

The panel is RGB565 with `LV_DITHER_GRADIENT` off. Three rules follow from that
and bind every UI colour choice — no gradients on surfaces, keep R = G, and
`CLR_HEX_TEXT_LOW` is the contrast floor. The measurements behind them are
recorded in the header comment above `CLR_HEX_SURFACE_0` in
`src/ui/ui_helpers.h`; read it before touching surface or text colours.

`lv_obj_set_style_transform_angle` is unusable on this target — see
[Troubleshooting → Known limitations](troubleshooting.md#known-limitations).
