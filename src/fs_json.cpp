// fs_json.cpp — see include/fs_json.h

#include "fs_json.h"
#include <Arduino.h>
#include <LittleFS.h>

bool fs_load_json(const char *path, JsonDocument &doc, const char *tag) {
  File f = LittleFS.open(path, "r");
  if (!f) {
    Serial.printf("[%s] Failed to open %s\n", tag, path);
    return false;
  }

  DeserializationError error = deserializeJson(doc, f);
  f.close();

  if (error) {
    Serial.printf("[%s] JSON parse failed for %s: %s\n", tag, path,
                  error.c_str());
    return false;
  }
  return true;
}

bool fs_save_json(const char *path, const JsonDocument &doc, const char *tag) {
  String tmp = String(path) + ".tmp";

  File f = LittleFS.open(tmp.c_str(), "w");
  if (!f) {
    Serial.printf("[%s] CRITICAL: cannot open %s for writing\n", tag,
                  tmp.c_str());
    return false;
  }

  size_t written = serializeJson(doc, f);
  f.close();

  if (written == 0) {
    Serial.printf("[%s] Failed to write %s\n", tag, tmp.c_str());
    LittleFS.remove(tmp.c_str());
    return false;
  }

  // Rename is the commit point: until it succeeds the old file is still the
  // one on disk.
  LittleFS.remove(path);
  if (!LittleFS.rename(tmp.c_str(), path)) {
    Serial.printf("[%s] CRITICAL: rename %s -> %s failed\n", tag, tmp.c_str(),
                  path);
    LittleFS.remove(tmp.c_str());
    return false;
  }
  return true;
}
