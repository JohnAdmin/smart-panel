#include "scene.h"
#include "config_json.h"
#include "fs_json.h"
#include "globals.h"
#include "mqtt_manager.h"
#include "ui.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <lvgl.h>

Scene scenes[MAX_SCENES];
int sceneCount = 0;

// Scene icon symbols — using LVGL built-in symbols
static const char *scene_icon_symbols[] = {
    LV_SYMBOL_OK,       // Morning  (sunrise)
    LV_SYMBOL_EYE_CLOSE,// Night    (moon)
    LV_SYMBOL_RIGHT,    // Leave    (door)
    LV_SYMBOL_VIDEO,    // Movie    (film)
    LV_SYMBOL_AUDIO,    // Party    (music)
    LV_SYMBOL_SETTINGS, // Custom   (gear)
};

static const char *scene_icon_names[] = {
    "Morning", "Night", "Leave", "Movie", "Party", "Custom",
};

const char *getSceneIconSymbol(int icon_index) {
  if (icon_index < 0 || icon_index >= 6)
    return LV_SYMBOL_SETTINGS;
  return scene_icon_symbols[icon_index];
}

const char *getSceneIconName(int icon_index) {
  if (icon_index < 0 || icon_index >= 6)
    return "Custom";
  return scene_icon_names[icon_index];
}

void loadScenes() {
  if (!LittleFS.begin(true)) {
    Serial.println("[SCENE] LittleFS mount failed");
    return;
  }

  if (!LittleFS.exists(FS_SCENES_JSON)) {
    Serial.println("[SCENE] No scenes.json found — starting empty");
    sceneCount = 0;
    return;
  }

  JsonDocument doc;
  if (!fs_load_json(FS_SCENES_JSON, doc, "SCENE")) {
    sceneCount = 0;
    return;
  }

  scenesFromJson(doc["scenes"]);

  Serial.printf("[SCENE] Loaded %d scenes OK\n", sceneCount);
}

bool saveScenes() {
  JsonDocument doc;
  scenesToJson(doc["scenes"].to<JsonArray>());

  if (!fs_save_json(FS_SCENES_JSON, doc, "SCENE"))
    return false;

  Serial.printf("[SCENE] Saved %d scenes OK\n", sceneCount);
  return true;
}

bool addScene(const char *name, int icon, uint32_t color) {
  if (sceneCount >= MAX_SCENES)
    return false;

  Scene &sc = scenes[sceneCount];
  strlcpy(sc.name, name, sizeof(sc.name));
  sc.icon_index = icon;
  sc.color = color;
  sc.action_count = 0;
  sceneCount++;
  return saveScenes();
}

void deleteScene(int index) {
  if (index < 0 || index >= sceneCount)
    return;
  for (int i = index; i < sceneCount - 1; i++) {
    scenes[i] = scenes[i + 1];
  }
  sceneCount--;
  saveScenes();
}

void executeScene(int index) {
  if (index < 0 || index >= sceneCount)
    return;

  Serial.printf("[SCENE] Executing '%s' (%d actions)\n", scenes[index].name,
                scenes[index].action_count);

  for (int i = 0; i < scenes[index].action_count; i++) {
    const char *topic = scenes[index].actions[i].topic;
    const char *payload = scenes[index].actions[i].payload;
    if (strlen(topic) > 0) {
      mqtt_publish_string(topic, payload);
      Serial.printf("  -> %s = %s\n", topic, payload);
    }
  }

  // Show toast — called from LVGL event context so don't take lvgl_mux
  static char toast_buf[48];
  snprintf(toast_buf, sizeof(toast_buf), LV_SYMBOL_PLAY " %s", scenes[index].name);
  ui_show_toast(toast_buf);
}
