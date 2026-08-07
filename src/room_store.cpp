#include "room.h"
#include "config.h"
#include "fs_json.h"
#include "globals.h"
#include "ui/ui_screens.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

Room rooms[MAX_ROOMS];
int  roomCount = 0;

// --------------------------------------------------------
//  Persistence
// --------------------------------------------------------

void loadRooms() {
  roomCount = 0;

  if (!LittleFS.exists(FS_ROOMS_JSON)) {
    // Not an error: every panel that predates rooms.json lands here, and
    // room_sync_from_devices() rebuilds the list from devices.json.
    Serial.println("[ROOM] No rooms.json — will derive rooms from devices");
    return;
  }

  JsonDocument doc;
  if (!fs_load_json(FS_ROOMS_JSON, doc, "ROOM"))
    return;

  for (JsonObject r : doc["rooms"].as<JsonArray>()) {
    if (roomCount >= MAX_ROOMS) break;
    const char *name = r["name"] | "";
    if (!name[0]) continue;
    Room &room = rooms[roomCount];
    room = Room();
    strncpy(room.name, name, sizeof(room.name) - 1);
    room.icon_type = r["icon_type"] | ROOM_ICON_AUTO;
    strncpy(room.climate_topic, r["climate_topic"] | "",
            sizeof(room.climate_topic) - 1);
    roomCount++;
  }
  Serial.printf("[ROOM] Loaded %d rooms from JSON OK\n", roomCount);
}

bool saveRooms() {
  JsonDocument doc;
  JsonArray arr = doc["rooms"].to<JsonArray>();
  for (int i = 0; i < roomCount; i++) {
    JsonObject r = arr.add<JsonObject>();
    r["name"] = rooms[i].name;
    r["icon_type"] = rooms[i].icon_type;
    r["climate_topic"] = rooms[i].climate_topic;
  }
  if (!fs_save_json(FS_ROOMS_JSON, doc, "ROOM"))
    return false;
  Serial.printf("[ROOM] Saved %d rooms to JSON OK\n", roomCount);
  return true;
}

// --------------------------------------------------------
//  Lookup / derivation
// --------------------------------------------------------

int room_find(const char *name) {
  if (!name || !name[0]) return -1;
  for (int i = 0; i < roomCount; i++)
    if (strcmp(rooms[i].name, name) == 0) return i;
  return -1;
}

bool room_sync_from_devices() {
  bool added = false;
  for (int i = 0; i < deviceCount && i < MAX_DEVICES; i++) {
    if (!ui_device_is_visible(i)) continue; // the panel's own entry has no room
    const char *name = devices[i].room;
    if (!name[0]) continue;
    if (room_find(name) >= 0) continue;
    if (roomCount >= MAX_ROOMS) {
      Serial.printf("[ROOM] Max %d rooms reached, ignoring '%s'\n", MAX_ROOMS,
                    name);
      break;
    }
    rooms[roomCount] = Room();
    strncpy(rooms[roomCount].name, name, sizeof(rooms[0].name) - 1);
    roomCount++;
    added = true;
    Serial.printf("[ROOM] Discovered room '%s'\n", name);
  }
  return added;
}

void room_count_devices(int room_idx, int *on_count, int *total) {
  int on = 0, n = 0;
  if (room_idx >= 0 && room_idx < roomCount) {
    for (int i = 0; i < deviceCount && i < MAX_DEVICES; i++) {
      if (!ui_device_is_visible(i)) continue;
      if (strcmp(devices[i].room, rooms[room_idx].name) != 0) continue;
      n++;
      if (devices[i].status) on++;
    }
  }
  if (on_count) *on_count = on;
  if (total) *total = n;
}

int room_effective_icon(int room_idx) {
  if (room_idx < 0 || room_idx >= roomCount) return ICON_GENERIC;
  if (rooms[room_idx].icon_type != ROOM_ICON_AUTO)
    return rooms[room_idx].icon_type;

  // Auto: whichever icon the room's devices use most. A room of lamps gets a
  // lamp, a garage of one door gets the door — without the user configuring
  // anything. Ties go to the lowest icon id, which is stable across rebuilds.
  int tally[10] = {0};
  const int n_icons = (int)(sizeof(tally) / sizeof(tally[0]));
  for (int i = 0; i < deviceCount && i < MAX_DEVICES; i++) {
    if (!ui_device_is_visible(i)) continue;
    if (strcmp(devices[i].room, rooms[room_idx].name) != 0) continue;
    int t = devices[i].icon_type;
    if (t >= 0 && t < n_icons) tally[t]++;
  }
  int best = ICON_GENERIC, best_n = 0;
  for (int t = 0; t < n_icons; t++)
    if (tally[t] > best_n) { best_n = tally[t]; best = t; }
  return best;
}
