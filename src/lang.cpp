// lang.cpp — Multi-language support
// Default English strings are embedded. Override via /lang_xx.json in LittleFS.

#include "lang.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

// Current language code
static char _lang[8] = "en";

// Available languages
const char *lang_codes[LANG_OPTIONS_COUNT] = {"en", "th"};
const char *lang_names[LANG_OPTIONS_COUNT] = {"English", "\xe0\xb9\x84\xe0\xb8\x97\xe0\xb8\xa2"}; // "ไทย" in UTF-8

// Default English strings (embedded — always available without LittleFS)
static const char *defaults[LANG_KEY_COUNT] = {
  // --- General ---
  [L_ON]            = "On",
  [L_OFF]           = "Off",
  [L_SAVE]          = "Save",
  [L_CANCEL]        = "Cancel",
  [L_DELETE]        = "Delete",
  [L_BACK]          = "Back",
  [L_YES]           = "Yes",
  [L_NO]            = "No",
  [L_OK]            = "OK",
  [L_ADD]           = "Add",
  [L_EDIT]          = "Edit",
  [L_CONNECTED]     = "Connected",
  [L_DISCONNECTED]  = "Disconnected",
  [L_LOADING]       = "Loading...",

  // --- Main Screen ---
  [L_NO_DEVICES]    = "No devices configured.\nAdd devices from the web portal.",
  [L_FAV_HINT]      = "Mark devices as favorites\n     from the web portal",
  [L_ALL_OFF]       = "All OFF",
  [L_CONFIRM_ALL_OFF] = "Turn everything off?",
  // %s is the room name
  [L_CONFIRM_ALL_OFF_MSG] = "Switch off every device in %s?",
  [L_NOTHING_ON]    = "Nothing is on",
  [L_FAVORITES]     = "Favorites",
  // %d is the number of devices currently on in that room
  [L_ON_COUNT]      = "%d on",
  [L_NONE_ON]       = "All off",
  [L_NO_ROOMS]      = "No rooms yet.\nGive your devices a room name\nin the web portal.",

  // --- Settings ---
  [L_SETTINGS]      = "Settings",
  [L_WEB_PORTAL]    = "Web Portal",
  [L_PANEL_SETTINGS]= "Panel Settings",
  [L_WIFI_SETUP]    = "Wi-Fi Setup",
  [L_WIFI_SSID]     = "WiFi SSID",
  [L_WIFI_PASSWORD] = "WiFi Password",
  [L_SCAN_QR]       = "Scan to open\nWeb Portal",
  [L_WIFI_NOT_CONNECTED] = "Wi-Fi is not connected.\nConfigure credentials to enable Web Portal.",
  [L_SETUP_WIFI]    = "Setup Wi-Fi",
  [L_BRIGHTNESS]    = "Brightness",
  [L_TIME_FORMAT]   = "Time Format",
  [L_TIME_12H]      = "12 Hour",
  [L_TIME_24H]      = "24 Hour",
  [L_SCREENSAVER]   = "Screensaver",
  [L_FLIP_CLOCK]    = "Flip",
  [L_MINIMAL]       = "Plain",
  [L_SCREEN_OFF]    = "Off",
  [L_SCREEN_TIMEOUT]= "Screen Timeout",
  [L_1_MIN]         = "1m",
  [L_2_MIN]         = "2m",
  [L_5_MIN]         = "5m",
  [L_NEVER]         = "Never",
  [L_RESET_PASS]    = "Reset Pass",
  [L_LANGUAGE]      = "Language",
  [L_HAPTIC]        = "Haptic",
  [L_DISPLAY]       = "Display",
  [L_SYSTEM]        = "System",
  [L_MANAGE]        = "Manage",
  [L_HOME_LAYOUT]   = "Home Layout",
  [L_LAYOUT_GRID]   = "Grid",
  [L_LAYOUT_LIST]   = "List",
  [L_WALLPAPER]     = "Wallpaper",

  [L_PANEL_NAME]    = "Panel Name",
  [L_PANEL_NAME_EMPTY] = "Name cannot be empty",
  [L_RESTART]       = "Restart",
  [L_CONFIRM_RESTART] = "Restart the panel now?",
  [L_FACTORY_RESET] = "Factory Reset",
  [L_CONFIRM_FACTORY_RESET] =
      "Erase all devices, rooms, scenes and settings?",
  [L_UPDATING]      = "Updating",

  // --- Device Manager ---
  [L_DEVICES]       = "Devices",
  [L_NEW_DEVICE]    = "New Device",
  [L_EDIT_DEVICE]   = "Edit Device",
  [L_DEVICE_NAME]   = "Device Name",
  [L_DEVICE_NAME_HINT] = "e.g. Desk Light",
  [L_STATE_TOPIC]   = "State Topic (getOn)",
  [L_STATE_TOPIC_HINT] = "homebridge/name/stat",
  [L_CMD_TOPIC]     = "Command Topic (setOn)",
  [L_CMD_TOPIC_HINT]= "homebridge/name/set",
  [L_DIMMER_TOPIC]  = "Dimmer Topic (optional)",
  [L_DIMMER_TOPIC_HINT] = "homebridge/name/dimmer",
  [L_ICON_TYPE]     = "Icon Type",
  [L_DEV_TYPE]      = "Type",
  [L_TYPE_TOGGLE]   = "Switch",
  [L_TYPE_DIMMER]   = "Dimmer",
  [L_TYPE_FAN]      = "Fan",
  [L_TYPE_AC]       = "AC",
  // %d is the fan speed, 1-3
  [L_SPEED]         = "Speed %d",
  [L_COOLING]       = "Cooling",
  [L_CONFIRM_DELETE]= "Confirm Delete",
  [L_CONFIRM_DELETE_MSG] = "Are you sure you want to delete this device?",
  [L_CONFIRM_DELETE_SCHED] = "Delete this schedule?",

  // --- Dimmer ---
  [L_BRIGHTNESS_CTRL] = "Brightness Control",
  [L_TAP_OUTSIDE_CLOSE] = "Tap outside to close",

  // --- Scenes ---
  [L_SCENES]        = "Scenes",
  [L_NEW_SCENE]     = "New Scene",
  [L_EDIT_SCENE]    = "Edit Scene",
  [L_NO_SCENES]     = "No scenes yet.\nTap '+' to create your first scene.",
  [L_ACTIONS]       = "actions",
  [L_SCENE_NAME]    = "Scene Name",
  [L_SCENE_NAME_HINT] = "e.g. Good Morning",
  [L_SCENE_ICON]    = "Icon",
  [L_SCENE_ICONS]   = "Morning\nNight\nLeave\nMovie\nParty\nCustom",
  [L_SCENE_ACTIONS] = "Actions (MQTT Topic \xe2\x86\x92 Payload)",
  [L_TOPIC]         = "Topic",
  [L_PAYLOAD]       = "Payload",
  [L_ACTION]        = "Action",
  [L_TAP_TO_RUN]    = "Tap to run",

  // --- Schedules ---
  [L_SCHEDULES]     = "Schedules",
  [L_SCHED]         = "Sched",
  [L_NO_SCHEDULES]  = "No schedules.\nUse Web Portal to add schedules.",
  [L_EVERY_DAY]     = "Every day",
  [L_UNKNOWN]       = "Unknown",
  [L_DAY_SU] = "Su", [L_DAY_MO] = "Mo", [L_DAY_TU] = "Tu",
  [L_DAY_WE] = "We", [L_DAY_TH] = "Th", [L_DAY_FR] = "Fr", [L_DAY_SA] = "Sa",

  // --- Sensors ---
  [L_SENSORS]       = "Sensors",
  [L_AVG_TEMP]      = "Avg Temp",
  [L_AVG_HUM]       = "Avg Humidity",
  [L_OUTDOOR]       = "Outdoor",
  [L_NO_SENSORS]    = "No room has a climate topic yet.\n"
                      "Add one per room in the web portal,\n"
                      "then restart the panel.",

  // --- Screensaver ---
  [L_SMART_HOME]    = "SMART HOME",
  [L_TAP_TO_WAKE]   = "Tap to unlock",

  // --- UI General ---
  [L_ICON_NAMES]    = "Lamp\nFan\nSwitch\nPlug\nThermostat\nLock\nTV\nGarage\nLight Strip\nGeneric",

  // --- Web Portal ---
  [L_WEB_TITLE]     = "Hero Home Configuration",
  [L_WEB_SAVE_RESTART] = "Save & Restart",
  [L_WEB_NETWORK]   = "Network Setup",
  [L_WEB_DEVICE_MGMT] = "Device Management",
  [L_WEB_SYSTEM]    = "System & Updates",
  [L_WEB_MQTT_CONFIG] = "Network & MQTT Configuration",
  [L_WEB_WIFI_CRED] = "Wi-Fi Credentials",
  [L_WEB_MQTT_SERVER] = "MQTT Server IP",
  [L_WEB_WEATHER_CITY] = "Weather City",
  [L_WEB_PANEL_TITLE] = "Panel Title",
  [L_WEB_TIMEZONE]  = "Timezone",
  [L_WEB_AUTH]      = "Web Portal Authentication",
  [L_WEB_WALLPAPER] = "Wallpaper & Screensaver",
  [L_WEB_SS_TIMER]  = "Screensaver Timer",
  [L_WEB_DEVICES]   = "Hardware Devices",
  [L_WEB_ADD_DEVICE]= "Add Device",
  [L_WEB_SEARCH]    = "Search devices by name, room, or topic...",
  [L_WEB_SCENES_TAB]= "Scenes",
  [L_WEB_SCHEDULES_TAB] = "Schedules",
  [L_WEB_ADD_SCENE] = "Add Scene",
  [L_WEB_SAVE_SCENES] = "Save Scenes",
  [L_WEB_SCENE_HELP]= "Scenes let you trigger multiple MQTT commands with one tap.",
  [L_WEB_ADD_SCHEDULE] = "+ Add Schedule",
  [L_WEB_SCHED_HELP]= "Trigger scenes automatically at scheduled times. Max 16 schedules.",
  [L_WEB_IP_ADDRESS]= "IP Address",
  [L_WEB_EXPORT_HELP] = "Export all settings, devices, scenes, and schedules as a JSON file.",
  [L_WEB_SAVED]     = "Configuration Saved! Rebooting...",
  [L_WEB_LOAD_FAIL] = "Failed to load configuration. Make sure Panel is online.",
  [L_WEB_MAX_DEVICES] = "Maximum system capacity (100 devices) reached.",
  [L_WEB_CONFIRM_DEL] = "Are you sure you want to delete this device?",
  [L_WEB_INVALID_MQTT] = "Invalid MQTT server address.",
};

// Override buffer — loaded from JSON
static const char *overrides[LANG_KEY_COUNT] = {};

// Key name map (for JSON field matching)
static const char *key_names[LANG_KEY_COUNT] = {
  [L_ON]="on", [L_OFF]="off", [L_SAVE]="save", [L_CANCEL]="cancel",
  [L_DELETE]="delete", [L_BACK]="back", [L_YES]="yes", [L_NO]="no",
  [L_OK]="ok", [L_ADD]="add", [L_EDIT]="edit", [L_CONNECTED]="connected",
  [L_DISCONNECTED]="disconnected", [L_LOADING]="loading",

  [L_NO_DEVICES]="no_devices", [L_FAV_HINT]="fav_hint",
  [L_ALL_OFF]="all_off", [L_CONFIRM_ALL_OFF]="confirm_all_off",
  [L_CONFIRM_ALL_OFF_MSG]="confirm_all_off_msg", [L_NOTHING_ON]="nothing_on",
  [L_FAVORITES]="favorites", [L_ON_COUNT]="on_count",
  [L_NONE_ON]="none_on", [L_NO_ROOMS]="no_rooms",

  [L_SETTINGS]="settings", [L_WEB_PORTAL]="web_portal",
  [L_PANEL_SETTINGS]="panel_settings", [L_WIFI_SETUP]="wifi_setup",
  [L_WIFI_SSID]="wifi_ssid", [L_WIFI_PASSWORD]="wifi_password",
  [L_SCAN_QR]="scan_qr", [L_WIFI_NOT_CONNECTED]="wifi_not_connected",
  [L_SETUP_WIFI]="setup_wifi", [L_BRIGHTNESS]="brightness",
  [L_TIME_FORMAT]="time_format", [L_TIME_12H]="time_12h",
  [L_TIME_24H]="time_24h", [L_SCREENSAVER]="screensaver",
  [L_FLIP_CLOCK]="flip_clock", [L_MINIMAL]="minimal",
  [L_SCREEN_OFF]="screen_off", [L_SCREEN_TIMEOUT]="screen_timeout",
  [L_1_MIN]="1_min", [L_2_MIN]="2_min", [L_5_MIN]="5_min",
  [L_NEVER]="never", [L_RESET_PASS]="reset_pass", [L_LANGUAGE]="language",
  [L_HAPTIC]="haptic", [L_DISPLAY]="display", [L_SYSTEM]="system",
  [L_MANAGE]="manage", [L_HOME_LAYOUT]="home_layout",
  [L_LAYOUT_GRID]="layout_grid", [L_LAYOUT_LIST]="layout_list",
  [L_WALLPAPER]="wallpaper",

  [L_PANEL_NAME]="panel_name", [L_PANEL_NAME_EMPTY]="panel_name_empty",
  [L_RESTART]="restart", [L_CONFIRM_RESTART]="confirm_restart",
  [L_FACTORY_RESET]="factory_reset",
  [L_CONFIRM_FACTORY_RESET]="confirm_factory_reset",
  [L_UPDATING]="updating",
  [L_DEVICES]="devices", [L_NEW_DEVICE]="new_device",
  [L_EDIT_DEVICE]="edit_device", [L_DEVICE_NAME]="device_name",
  [L_DEVICE_NAME_HINT]="device_name_hint",
  [L_STATE_TOPIC]="state_topic", [L_STATE_TOPIC_HINT]="state_topic_hint",
  [L_CMD_TOPIC]="cmd_topic", [L_CMD_TOPIC_HINT]="cmd_topic_hint",
  [L_DIMMER_TOPIC]="dimmer_topic", [L_DIMMER_TOPIC_HINT]="dimmer_topic_hint",
  [L_ICON_TYPE]="icon_type", [L_DEV_TYPE]="dev_type",
  [L_TYPE_TOGGLE]="type_toggle", [L_TYPE_DIMMER]="type_dimmer",
  [L_TYPE_FAN]="type_fan", [L_TYPE_AC]="type_ac",
  [L_SPEED]="speed", [L_COOLING]="cooling",
  [L_CONFIRM_DELETE]="confirm_delete",
  [L_CONFIRM_DELETE_MSG]="confirm_delete_msg",
  [L_CONFIRM_DELETE_SCHED]="confirm_delete_sched",

  [L_BRIGHTNESS_CTRL]="brightness_ctrl",
  [L_TAP_OUTSIDE_CLOSE]="tap_outside_close",

  [L_SCENES]="scenes", [L_NEW_SCENE]="new_scene",
  [L_EDIT_SCENE]="edit_scene", [L_NO_SCENES]="no_scenes",
  [L_ACTIONS]="actions", [L_SCENE_NAME]="scene_name",
  [L_SCENE_NAME_HINT]="scene_name_hint", [L_SCENE_ICON]="scene_icon",
  [L_SCENE_ICONS]="scene_icons", [L_SCENE_ACTIONS]="scene_actions",
  [L_TOPIC]="topic", [L_PAYLOAD]="payload",
  [L_ACTION]="action", [L_TAP_TO_RUN]="tap_to_run",

  [L_SCHEDULES]="schedules", [L_SCHED]="sched",
  [L_NO_SCHEDULES]="no_schedules",
  [L_EVERY_DAY]="every_day", [L_UNKNOWN]="unknown",
  [L_DAY_SU]="day_su", [L_DAY_MO]="day_mo", [L_DAY_TU]="day_tu",
  [L_DAY_WE]="day_we", [L_DAY_TH]="day_th", [L_DAY_FR]="day_fr", [L_DAY_SA]="day_sa",

  [L_SENSORS]="sensors", [L_AVG_TEMP]="avg_temp", [L_AVG_HUM]="avg_hum",
  [L_OUTDOOR]="outdoor", [L_NO_SENSORS]="no_sensors",
  [L_SMART_HOME]="smart_home", [L_TAP_TO_WAKE]="tap_to_wake",
  [L_ICON_NAMES]="icon_names",

  [L_WEB_TITLE]="web_title", [L_WEB_SAVE_RESTART]="web_save_restart",
  [L_WEB_NETWORK]="web_network", [L_WEB_DEVICE_MGMT]="web_device_mgmt",
  [L_WEB_SYSTEM]="web_system", [L_WEB_MQTT_CONFIG]="web_mqtt_config",
  [L_WEB_WIFI_CRED]="web_wifi_cred", [L_WEB_MQTT_SERVER]="web_mqtt_server",
  [L_WEB_WEATHER_CITY]="web_weather_city", [L_WEB_PANEL_TITLE]="web_panel_title",
  [L_WEB_TIMEZONE]="web_timezone", [L_WEB_AUTH]="web_auth",
  [L_WEB_WALLPAPER]="web_wallpaper", [L_WEB_SS_TIMER]="web_ss_timer",
  [L_WEB_DEVICES]="web_devices", [L_WEB_ADD_DEVICE]="web_add_device",
  [L_WEB_SEARCH]="web_search", [L_WEB_SCENES_TAB]="web_scenes_tab",
  [L_WEB_SCHEDULES_TAB]="web_schedules_tab",
  [L_WEB_ADD_SCENE]="web_add_scene", [L_WEB_SAVE_SCENES]="web_save_scenes",
  [L_WEB_SCENE_HELP]="web_scene_help",
  [L_WEB_ADD_SCHEDULE]="web_add_schedule", [L_WEB_SCHED_HELP]="web_sched_help",
  [L_WEB_IP_ADDRESS]="web_ip_address", [L_WEB_EXPORT_HELP]="web_export_help",
  [L_WEB_SAVED]="web_saved", [L_WEB_LOAD_FAIL]="web_load_fail",
  [L_WEB_MAX_DEVICES]="web_max_devices", [L_WEB_CONFIRM_DEL]="web_confirm_del",
  [L_WEB_INVALID_MQTT]="web_invalid_mqtt",
};

// Dynamic string buffer for JSON-loaded strings (in PSRAM if available)
static char *_strPool = nullptr;
static size_t _strPoolUsed = 0;
#define STR_POOL_SIZE 4096

static char *pool_alloc(const char *src) {
  if (!_strPool) {
    _strPool = (char *)ps_malloc(STR_POOL_SIZE);
    if (!_strPool) _strPool = (char *)malloc(STR_POOL_SIZE);
    if (!_strPool) return nullptr;
    _strPoolUsed = 0;
  }
  size_t len = strlen(src) + 1;
  if (_strPoolUsed + len > STR_POOL_SIZE) return nullptr; // pool full
  char *dst = _strPool + _strPoolUsed;
  memcpy(dst, src, len);
  _strPoolUsed += len;
  return dst;
}

const char *L(LangKey key) {
  if (key >= LANG_KEY_COUNT) return "?";
  if (overrides[key]) return overrides[key];
  return defaults[key] ? defaults[key] : "?";
}

const char *lang_current() { return _lang; }

void lang_load(const char *langCode) {
  strncpy(_lang, langCode, sizeof(_lang) - 1);
  _lang[sizeof(_lang) - 1] = '\0';

  // Reset overrides
  memset(overrides, 0, sizeof(overrides));
  if (_strPool) { _strPoolUsed = 0; }

  // English uses embedded defaults — no file needed
  if (strcmp(langCode, "en") == 0) {
    Serial.println("[LANG] English (default)");
    return;
  }

  // Build file path: /lang_th.json
  char path[24];
  snprintf(path, sizeof(path), "/lang_%s.json", langCode);

  File f = LittleFS.open(path, "r");
  if (!f) {
    Serial.printf("[LANG] File %s not found, using English\n", path);
    strncpy(_lang, "en", sizeof(_lang));
    return;
  }

  // Parse JSON
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Serial.printf("[LANG] JSON parse error: %s\n", err.c_str());
    strncpy(_lang, "en", sizeof(_lang));
    return;
  }

  int loaded = 0;
  for (int i = 0; i < LANG_KEY_COUNT; i++) {
    if (!key_names[i]) continue;
    const char *val = doc[key_names[i]];
    if (val) {
      char *copy = pool_alloc(val);
      if (copy) {
        overrides[i] = copy;
        loaded++;
      }
    }
  }

  Serial.printf("[LANG] Loaded %s: %d/%d strings (pool %u/%u bytes)\n",
                path, loaded, LANG_KEY_COUNT, _strPoolUsed, STR_POOL_SIZE);
}
