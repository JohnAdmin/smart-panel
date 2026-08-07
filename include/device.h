#pragma once
#include <Arduino.h>
#include <string.h>
#include "../src/config.h"

// What kind of control a device gets on its tile. Values are persisted in
// devices.json, so only append.
enum DevType : uint8_t {
  DEV_TOGGLE = 0, // on/off only — tap the tile
  DEV_DIMMER,     // 0-100 brightness
  DEV_FAN,        // speed 0-3, where 0 also means off
  DEV_AC,         // setpoint 18-30 °C
  DEV_TYPE_COUNT
};

// Represents a single smart home device and its runtime state
struct Device {
  char name[48];
  char room[48];
  char state_topic[64];
  char cmnd_topic[64];
  // The numeric channel. Historically dimmer-only, hence the name and the
  // devices.json key — both kept so existing configs load untouched — but what
  // the number *means* now depends on type: brightness, fan speed, or target
  // temperature. `brightness` below is the matching runtime value.
  char dimmer_topic[64];
  int icon_type;
  uint8_t dev_type;
  bool is_favorite;

  // Runtime state (Not saved to NVS)
  bool status;
  int brightness; // 0-100
  unsigned long lastToggleTime;
  unsigned long lastDimTime;
  unsigned long lastSeenTime; // millis() of last MQTT message received
  // Set when a command is published or queued for this device; cleared as soon
  // as the broker says anything back on one of its topics. Non-zero means
  // "we asked, nobody has confirmed yet" — the tile shows that as pending
  // rather than pretending the change already took effect.
  unsigned long pendingSince;

  Device() {
    memset(name, 0, sizeof(name));
    memset(room, 0, sizeof(room));
    memset(state_topic, 0, sizeof(state_topic));
    memset(cmnd_topic, 0, sizeof(cmnd_topic));
    memset(dimmer_topic, 0, sizeof(dimmer_topic));
    icon_type = 0;
    dev_type = DEV_TOGGLE;
    is_favorite = false;
    status = false;
    brightness = 50;
    lastToggleTime = 0;
    lastDimTime = 0;
    lastSeenTime = 0;
    pendingSince = 0;
  }

  void updateState(bool newState) { status = newState; }

  // ── The numeric channel, per type ───────────────────────
  // Only these three carry a level; a plain toggle has nothing to send.
  bool hasLevel() const {
    return dev_type == DEV_DIMMER || dev_type == DEV_FAN || dev_type == DEV_AC;
  }
  int levelMin() const { return dev_type == DEV_AC ? 18 : 0; }
  int levelMax() const {
    switch (dev_type) {
    case DEV_FAN: return 3;
    case DEV_AC:  return 30;
    default:      return 100;
    }
  }
  int clampLevel(int v) const {
    if (v < levelMin()) return levelMin();
    if (v > levelMax()) return levelMax();
    return v;
  }
  // A fan at speed 0 is off, and raising a dimmer or a fan off zero implies
  // switching it on. An AC setpoint says nothing about power either way.
  bool levelImpliesOn(int v) const {
    return dev_type != DEV_AC && v > 0;
  }
  bool levelImpliesOff(int v) const {
    return dev_type == DEV_FAN && v == 0;
  }

  // True when this entry mirrors the panel's own availability topic. It exists
  // so Homebridge can see the panel as a switch; it is not a controllable
  // device, so the dashboard hides it from tiles, room tabs and the ON counts.
  // It stays in the device list (Settings → Devices) so it can still be edited
  // or removed.
  bool isPanelStatus() const {
    return strcmp(state_topic, PANEL_STATUS_TOPIC) == 0;
  }

  void markToggled() { lastToggleTime = millis(); }

  void clearDebounce() { lastToggleTime = 0; }

  bool isDebouncing() const {
    return (millis() - lastToggleTime < DEVICE_DEBOUNCE_MS);
  }

  // A command is outstanding and the grace period has elapsed without the
  // broker confirming it. Devices with no state topic can never confirm, so
  // they are never reported as pending.
  bool isPending() const {
    return state_topic[0] != '\0' && pendingSince != 0 &&
           (millis() - pendingSince) > DEVICE_PENDING_GRACE_MS;
  }
};
