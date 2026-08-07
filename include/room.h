#pragma once
#include "../src/config.h"
#include <Arduino.h>
#include <string.h>

// A room, as the Home screen understands it.
//
// Rooms used to exist only as a string on each Device, rebuilt by scanning
// devices[] every time the grid was drawn. That was enough for a row of tabs
// but not for a room-first Home, which needs somewhere to hang a per-room
// icon and climate reading.
//
// `name` is still the join key — it matches Device.room verbatim, so the web
// portal's free-text room field and every existing devices.json keep working
// untouched. Rooms are discovered from devices (room_sync_from_devices) and
// only persisted so that per-room settings survive; a room never has to be
// created by hand before its devices can appear.
struct Room {
  char name[48];          // == Device.room
  int  icon_type;         // ICON_* from config.h, or ROOM_ICON_AUTO
  char climate_topic[64]; // MQTT topic carrying {"t":..,"h":..} — see below

  // --- Runtime, not persisted ---
  float temp;
  int   hum;
  bool  climateValid;

  Room() {
    memset(name, 0, sizeof(name));
    memset(climate_topic, 0, sizeof(climate_topic));
    icon_type = -1;
    temp = 0.0f;
    hum = 0;
    climateValid = false;
  }
};

// Derive the card icon from whatever devices the room holds, rather than
// making the user pick one. Any room that has never been edited uses this.
#define ROOM_ICON_AUTO (-1)

#define MAX_ROOMS 16

extern Room rooms[MAX_ROOMS];
extern int  roomCount;

// Reads /rooms.json. An absent file is a normal first boot — the caller is
// expected to follow with room_sync_from_devices().
void loadRooms();
bool saveRooms();

// Adds a Room for every distinct Device.room not already present, in the order
// the devices appear. Returns true if anything was added, which is the
// caller's cue to save. Never removes a room: one that has lost its last
// device may still be carrying an icon or climate topic the user chose, and
// the Home screen already skips rooms with nothing in them.
bool room_sync_from_devices();

// Index into rooms[] for `name`, or -1. Comparison is exact, matching how
// devices are grouped.
int room_find(const char *name);

// Counts this room's controllable devices — the panel's own status entry is
// excluded, same as everywhere else on the dashboard.
void room_count_devices(int room_idx, int *on_count, int *total);

// The icon to draw on the room's card: the room's own if it has one, else the
// most common icon among its devices.
int room_effective_icon(int room_idx);
