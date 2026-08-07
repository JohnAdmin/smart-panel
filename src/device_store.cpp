#include "config.h"
#include "device.h"
#include "fs_json.h"
#include "globals.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include <ArduinoJson.h>

volatile bool deviceStatesDirty = false;

// --------------------------------------------------------
//  Device Management — Persistence Logic
// --------------------------------------------------------

static void loadDevicesLegacyBin() {
  File f = LittleFS.open(FS_DEVICES_BIN, "r");
  if (!f) return;

  size_t read_bytes = f.read((uint8_t*)&deviceCount, sizeof(deviceCount));
  
  if (read_bytes != sizeof(deviceCount) || deviceCount < 0 || deviceCount > MAX_DEVICES) {
    deviceCount = 0;
    f.close();
    return;
  }

  if (deviceCount > 0) {
    f.read((uint8_t*)devices, sizeof(Device) * deviceCount);
  }
  f.close();
  Serial.printf("[DEV] Migrated %d devices from legacy devices.bin\n", deviceCount);
}

void loadDevices() {
  if (!LittleFS.begin(true)) {
    Serial.println("[DEV] LittleFS mount failed in loadDevices");
    return;
  }

  if (!LittleFS.exists(FS_DEVICES_JSON)) {
    if (LittleFS.exists(FS_DEVICES_BIN)) {
      Serial.println("[DEV] Found legacy devices.bin, migrating to JSON...");
      loadDevicesLegacyBin();
      saveDevices(); // Save as JSON
      return;
    }
    Serial.println("[DEV] No device storage found (clean boot or wiped)");
    deviceCount = 0;
    return;
  }

  JsonDocument doc;
  if (!fs_load_json(FS_DEVICES_JSON, doc, "DEV")) {
    deviceCount = 0;
    return;
  }

  JsonArray devs = doc["devices"];
  deviceCount = 0;

  for (JsonObject dev : devs) {
    if (deviceCount >= MAX_DEVICES) break;

    // We use a temporary save flag=false to just populate memory efficiently
    addDevice(
      dev["name"] | "Unknown",
      dev["room"] | "Room",
      dev["state_topic"] | "",
      dev["cmnd_topic"] | "",
      dev["dimmer_topic"] | "",
      dev["icon_type"] | 0,
      dev["is_favorite"] | false,
      false 
    );
    // Restore persisted state
    devices[deviceCount - 1].status = dev["status"] | false;
    devices[deviceCount - 1].brightness = dev["brightness"] | 0;

    // dev_type predates nothing — every config written before device types
    // existed lacks the key. Infer it the way the old UI behaved: a device
    // with a dimmer topic got the brightness modal, everything else was a
    // plain toggle. That leaves existing panels working identically until the
    // owner picks a type in the portal.
    Device &d = devices[deviceCount - 1];
    if (dev["dev_type"].is<int>()) {
      int t = dev["dev_type"].as<int>();
      d.dev_type = (t >= 0 && t < DEV_TYPE_COUNT) ? (uint8_t)t : DEV_TOGGLE;
    } else {
      d.dev_type = d.dimmer_topic[0] ? DEV_DIMMER : DEV_TOGGLE;
    }
  }

  Serial.printf("[DEV] Loaded %d devices from JSON OK\n", deviceCount);
}

bool saveDevices() {
  Serial.printf("[DEV] Starting save for %d devices to JSON\n", deviceCount);
  safe_wdt_reset(); 
  
  JsonDocument doc;
  JsonArray devs = doc["devices"].to<JsonArray>();

  for (int i = 0; i < deviceCount; i++) {
    JsonObject dev = devs.add<JsonObject>();
    dev["name"] = devices[i].name;
    dev["room"] = devices[i].room;
    dev["state_topic"] = devices[i].state_topic;
    dev["cmnd_topic"] = devices[i].cmnd_topic;
    dev["dimmer_topic"] = devices[i].dimmer_topic;
    dev["icon_type"] = devices[i].icon_type;
    dev["dev_type"] = devices[i].dev_type;
    dev["is_favorite"] = devices[i].is_favorite;
    dev["status"] = devices[i].status;
    dev["brightness"] = devices[i].brightness;
  }

  if (!fs_save_json(FS_DEVICES_JSON, doc, "DEV"))
    return false;
  safe_wdt_reset();

  Serial.printf("[DEV] Saved %d devices to JSON OK\n", deviceCount);
  return true;
}

bool addDevice(const char *name, const char *room, const char *stat,
               const char *cmnd, const char *dimmer, int icon, bool isFav,
               bool save) {

  if (deviceCount >= MAX_DEVICES) {
    Serial.println("[DEV] Max device limit reached");
    return false;
  }

  // Populate memory struct
  strncpy(devices[deviceCount].name, name, sizeof(devices[deviceCount].name) - 1);
  devices[deviceCount].name[sizeof(devices[deviceCount].name) - 1] = '\0';

  strncpy(devices[deviceCount].room, room, sizeof(devices[0].room) - 1);
  devices[deviceCount].room[sizeof(devices[0].room) - 1] = '\0';

  strncpy(devices[deviceCount].state_topic, stat, sizeof(devices[0].state_topic) - 1);
  devices[deviceCount].state_topic[sizeof(devices[0].state_topic) - 1] = '\0';

  strncpy(devices[deviceCount].cmnd_topic, cmnd, sizeof(devices[0].cmnd_topic) - 1);
  devices[deviceCount].cmnd_topic[sizeof(devices[0].cmnd_topic) - 1] = '\0';

  strncpy(devices[deviceCount].dimmer_topic, dimmer, sizeof(devices[0].dimmer_topic) - 1);
  devices[deviceCount].dimmer_topic[sizeof(devices[0].dimmer_topic) - 1] = '\0';

  devices[deviceCount].icon_type = icon;
  devices[deviceCount].is_favorite = isFav;
  devices[deviceCount].status = false; // default OFF

  deviceCount++;
  Serial.printf("[DEV] Added: %s (%s). Count: %d (Batch: %s)\n", name, room,
                deviceCount, save ? "No" : "Yes");

  if (save) {
    return saveDevices();
  }
  return true;
}

void deleteDevice(int index) {
  if (index < 0 || index >= deviceCount)
    return;

  // Unsubscribe MQTT topics before removing
  if (mqttClient.connected()) {
    if (strlen(devices[index].state_topic) > 0)
      mqttClient.unsubscribe(devices[index].state_topic);
    if (strlen(devices[index].cmnd_topic) > 0)
      mqttClient.unsubscribe(devices[index].cmnd_topic);
    if (strlen(devices[index].dimmer_topic) > 0)
      mqttClient.unsubscribe(devices[index].dimmer_topic);
  }

  // Shift items down
  for (int i = index; i < deviceCount - 1; i++) {
    devices[i] = devices[i + 1];
  }
  deviceCount--;
  saveDevices();
}
