#include "schedule.h"
#include "config_json.h"
#include "fs_json.h"
#include "scene.h"
#include "globals.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <time.h>

Schedule schedules[MAX_SCHEDULES];
int scheduleCount = 0;

// Track which minute was last executed so we fire only once per minute
static int lastCheckedMinute = -1;

void loadSchedules() {
  if (!LittleFS.begin(true)) {
    Serial.println("[SCHED] LittleFS mount failed");
    return;
  }

  if (!LittleFS.exists(FS_SCHEDULES_JSON)) {
    Serial.println("[SCHED] No schedules.json found — starting empty");
    scheduleCount = 0;
    return;
  }

  JsonDocument doc;
  if (!fs_load_json(FS_SCHEDULES_JSON, doc, "SCHED")) {
    scheduleCount = 0;
    return;
  }

  schedulesFromJson(doc["schedules"]);

  Serial.printf("[SCHED] Loaded %d schedules OK\n", scheduleCount);
}

bool saveSchedules() {
  JsonDocument doc;
  schedulesToJson(doc["schedules"].to<JsonArray>());

  if (!fs_save_json(FS_SCHEDULES_JSON, doc, "SCHED"))
    return false;

  Serial.printf("[SCHED] Saved %d schedules OK\n", scheduleCount);
  return true;
}

bool addSchedule(int sceneIdx, uint8_t h, uint8_t m, uint8_t days) {
  if (scheduleCount >= MAX_SCHEDULES) return false;

  Schedule &sc = schedules[scheduleCount];
  sc.scene_index = sceneIdx;
  sc.hour = h;
  sc.minute = m;
  sc.days = days;
  sc.enabled = true;
  scheduleCount++;
  return saveSchedules();
}

void deleteSchedule(int index) {
  if (index < 0 || index >= scheduleCount) return;
  for (int i = index; i < scheduleCount - 1; i++) {
    schedules[i] = schedules[i + 1];
  }
  scheduleCount--;
  saveSchedules();
}

void checkSchedules() {
  if (scheduleCount == 0) return;

  time_t now_ts;
  time(&now_ts);
  struct tm ti;
  localtime_r(&now_ts, &ti);

  // Only valid time
  if (ti.tm_year < 100) return;

  int curMinute = ti.tm_hour * 60 + ti.tm_min;

  // Already checked this minute — skip
  if (curMinute == lastCheckedMinute) return;
  lastCheckedMinute = curMinute;

  // tm_wday: 0=Sun, 1=Mon, ... 6=Sat — matches our bitmask
  uint8_t todayBit = (1 << ti.tm_wday);

  for (int i = 0; i < scheduleCount; i++) {
    if (!schedules[i].enabled) continue;
    if (schedules[i].hour != ti.tm_hour) continue;
    if (schedules[i].minute != ti.tm_min) continue;
    if (!(schedules[i].days & todayBit)) continue;

    int si = schedules[i].scene_index;
    if (si >= 0 && si < sceneCount) {
      Serial.printf("[SCHED] Firing schedule #%d -> scene '%s' at %02d:%02d\n",
                    i, scenes[si].name, ti.tm_hour, ti.tm_min);
      executeScene(si);
    }
  }
}
