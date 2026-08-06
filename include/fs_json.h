#pragma once
// fs_json.h
// Shared LittleFS <-> ArduinoJson plumbing for the three persisted config
// files (devices.json, scenes.json, schedules.json), which previously carried
// three verbatim copies of the same open/parse and write/rename sequences.
//
// Callers own mounting the filesystem and deciding what an absent file means
// (a clean boot, a migration from a legacy format, ...), so neither helper
// treats "not found" as its own concern.

#include <ArduinoJson.h>

// Reads and parses `path` into `doc`. `tag` prefixes log lines, e.g. "DEV".
// Returns false if the file cannot be opened or does not parse; `doc` is then
// left in whatever state deserializeJson() produced and must not be trusted.
bool fs_load_json(const char *path, JsonDocument &doc, const char *tag);

// Serializes `doc` to `path`, writing to a `.tmp` sibling and renaming it into
// place. A power cut mid-write therefore leaves the previous file intact
// instead of a truncated one. Returns false on any failure, having cleaned up
// the temporary file.
bool fs_save_json(const char *path, const JsonDocument &doc, const char *tag);
