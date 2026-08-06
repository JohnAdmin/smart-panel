#pragma once
// config_json.h
// One definition of the JSON shape of each persisted config array, shared by
// the LittleFS stores (scene_manager.cpp, schedule_manager.cpp) and the web
// API routes (web_server.cpp).
//
// Each of those four places used to carry its own copy of the field mapping,
// so adding a field meant editing all four and the copies had already begun to
// drift: the store's scene loader skipped the `sc = Scene()` reset and the
// `actions` type check that the web route performed, and the web route's day
// mask defaulted to a bare 127 instead of DAY_ALL.
//
// These operate on the global scenes[]/sceneCount and schedules[]/
// scheduleCount arrays, matching how the stores already work.

#include "scene.h"
#include "schedule.h"
#include <ArduinoJson.h>

// Append every populated entry to `out`.
void scenesToJson(JsonArray out);
void schedulesToJson(JsonArray out);

// Replace the in-memory array from `in`, bounded by MAX_SCENES / MAX_SCHEDULES,
// and update the matching count. Returns the new count. Entries missing a field
// take the documented default rather than whatever the slot held before.
int scenesFromJson(JsonArrayConst in);
int schedulesFromJson(JsonArrayConst in);
