# Configuration & Limits

[← Docs index](README.md)

## System Limits

Defined in `src/config.h`.

| Resource | Limit |
|----------|-------|
| Devices | 100 |
| Rooms | 16 |
| Scenes | 8 |
| Schedules | 16 |
| Actions per Scene | 10 |
| MQTT Offline Queue | 16 commands |
| MQTT Buffer | 512 bytes |
| Device Name / Room | 24 chars |
| Topic Strings | 64 chars |
| Scene Name | 24 chars |

## Default Configuration

| Setting | Default |
|---------|---------|
| WiFi | SC01-Plus-Setup (AP mode if no config) |
| MQTT Port | 1883 |
| mDNS | smartpanel.local |
| Web Auth | admin / admin |
| Theme | Dark |
| Home Layout | Grid (3-column room cards) |
| Grid Layout | Normal (3-column) |
| Time Format | 24-hour |
| Timezone | GMT+7 |
| Weather City | Bangkok |
| Screensaver | Flip Clock, 2 min timeout |
| Brightness | 255 (max) |

Everything above is editable from the panel's Settings screen or the
[web portal](web-api.md). Credentials are stored in NVS encrypted with a
MAC-based XOR + Base64.

To wipe all of it back to these defaults, see
[Troubleshooting → Factory reset](troubleshooting.md#factory-reset-touch--hold-during-boot).
