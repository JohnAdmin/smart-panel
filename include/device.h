#pragma once
#include <Arduino.h>
#include <string.h>
#include "../src/config.h"

// Represents a single smart home device and its runtime state
struct Device {
  char name[48];
  char room[48];
  char state_topic[64];
  char cmnd_topic[64];
  char dimmer_topic[64];
  int icon_type;
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
    is_favorite = false;
    status = false;
    brightness = 50;
    lastToggleTime = 0;
    lastDimTime = 0;
    lastSeenTime = 0;
    pendingSince = 0;
  }

  void updateState(bool newState) { status = newState; }

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
