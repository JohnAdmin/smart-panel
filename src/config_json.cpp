// config_json.cpp — see include/config_json.h

#include "config_json.h"

// Defaults applied when a field is absent from the incoming JSON. The Scene /
// Schedule constructors supply the same values for a freshly created entry;
// these named constants keep the two in step.
#define SCENE_JSON_DEFAULT_NAME  "Scene"
#define SCENE_JSON_DEFAULT_ICON  0

// ── Scene ───────────────────────────────────────────────────────────────────

static void sceneToJson(const Scene &sc, JsonObject out) {
  out["name"] = sc.name;
  out["icon"] = sc.icon_index;
  out["color"] = sc.color;

  JsonArray acts = out["actions"].to<JsonArray>();
  for (int i = 0; i < sc.action_count; i++) {
    JsonObject a = acts.add<JsonObject>();
    a["topic"] = sc.actions[i].topic;
    a["payload"] = sc.actions[i].payload;
  }
}

static void sceneFromJson(JsonObjectConst in, Scene &sc) {
  sc = Scene(); // clear stale actions left by a longer previous scene
  strlcpy(sc.name, in["name"] | SCENE_JSON_DEFAULT_NAME, sizeof(sc.name));
  sc.icon_index = in["icon"] | SCENE_JSON_DEFAULT_ICON;
  sc.color = in["color"] | (uint32_t)SCENE_DEFAULT_COLOR;
  sc.action_count = 0;

  if (!in["actions"].is<JsonArrayConst>())
    return;
  for (JsonObjectConst a : in["actions"].as<JsonArrayConst>()) {
    if (sc.action_count >= MAX_SCENE_ACTIONS)
      break;
    strlcpy(sc.actions[sc.action_count].topic, a["topic"] | "",
            sizeof(sc.actions[0].topic));
    strlcpy(sc.actions[sc.action_count].payload, a["payload"] | "",
            sizeof(sc.actions[0].payload));
    sc.action_count++;
  }
}

void scenesToJson(JsonArray out) {
  for (int i = 0; i < sceneCount; i++)
    sceneToJson(scenes[i], out.add<JsonObject>());
}

int scenesFromJson(JsonArrayConst in) {
  sceneCount = 0;
  for (JsonObjectConst s : in) {
    if (sceneCount >= MAX_SCENES)
      break;
    sceneFromJson(s, scenes[sceneCount]);
    sceneCount++;
  }
  return sceneCount;
}

// ── Schedule ────────────────────────────────────────────────────────────────

static void scheduleToJson(const Schedule &sc, JsonObject out) {
  out["scene"] = sc.scene_index;
  out["hour"] = sc.hour;
  out["minute"] = sc.minute;
  out["days"] = sc.days;
  out["enabled"] = sc.enabled;
}

static void scheduleFromJson(JsonObjectConst in, Schedule &sc) {
  // Every default lives in the Schedule constructor; read through it so the
  // JSON defaults cannot drift from the in-memory ones.
  sc = Schedule();
  sc.scene_index = in["scene"] | sc.scene_index;
  sc.hour = in["hour"] | sc.hour;
  sc.minute = in["minute"] | sc.minute;
  sc.days = in["days"] | sc.days;
  sc.enabled = in["enabled"] | sc.enabled;
}

void schedulesToJson(JsonArray out) {
  for (int i = 0; i < scheduleCount; i++)
    scheduleToJson(schedules[i], out.add<JsonObject>());
}

int schedulesFromJson(JsonArrayConst in) {
  scheduleCount = 0;
  for (JsonObjectConst s : in) {
    if (scheduleCount >= MAX_SCHEDULES)
      break;
    scheduleFromJson(s, schedules[scheduleCount]);
    scheduleCount++;
  }
  return scheduleCount;
}
