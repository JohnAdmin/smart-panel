#pragma once
#include <Arduino.h>
#include <string.h>

#define MAX_SCENES 8
#define MAX_SCENE_ACTIONS 10

// Accent color for a scene that doesn't specify one (premium amber).
#define SCENE_DEFAULT_COLOR 0xF59E0B

// A single action within a scene: publish payload to MQTT topic
struct SceneAction {
  char topic[64];
  char payload[32];

  SceneAction() {
    memset(topic, 0, sizeof(topic));
    memset(payload, 0, sizeof(payload));
  }
};

// Scene: a named group of MQTT actions executed together
struct Scene {
  char name[24];
  int icon_index;  // index into scene_icons[] — 0=Morning, 1=Night, etc.
  uint32_t color;  // accent color as 0xRRGGBB
  SceneAction actions[MAX_SCENE_ACTIONS];
  int action_count;

  Scene() {
    memset(name, 0, sizeof(name));
    icon_index = 0;
    color = SCENE_DEFAULT_COLOR;
    action_count = 0;
  }
};

// Scene icon labels (LVGL symbols for now)
// Index 0–5 map to these descriptive names
#define SCENE_ICON_MORNING   0
#define SCENE_ICON_NIGHT     1
#define SCENE_ICON_LEAVE     2
#define SCENE_ICON_MOVIE     3
#define SCENE_ICON_PARTY     4
#define SCENE_ICON_CUSTOM    5

extern Scene scenes[MAX_SCENES];
extern int sceneCount;

void loadScenes();
bool saveScenes();
bool addScene(const char *name, int icon, uint32_t color);
void deleteScene(int index);
void executeScene(int index);
const char *getSceneIconSymbol(int icon_index);
const char *getSceneIconName(int icon_index);
