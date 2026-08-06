#pragma once
// lang.h — Multi-language support via JSON files in LittleFS
// Default: English (embedded). Override by placing /lang_xx.json in LittleFS.

#include <Arduino.h>

// Language string keys — used as indices into the translation table.
// Keep alphabetical within category for maintainability.
enum LangKey : uint8_t {
  // --- General ---
  L_ON = 0,
  L_OFF,
  L_SAVE,
  L_CANCEL,
  L_DELETE,
  L_BACK,
  L_YES,
  L_NO,
  L_OK,
  L_ADD,
  L_EDIT,
  L_CONNECTED,
  L_DISCONNECTED,
  L_LOADING,

  // --- Main Screen ---
  L_NO_DEVICES,
  L_FAV_HINT,
  L_ALL_OFF,
  L_CONFIRM_ALL_OFF,
  L_CONFIRM_ALL_OFF_MSG,
  L_NOTHING_ON,

  // --- Settings ---
  L_SETTINGS,
  L_WEB_PORTAL,
  L_PANEL_SETTINGS,
  L_WIFI_SETUP,
  L_WIFI_SSID,
  L_WIFI_PASSWORD,
  L_SCAN_QR,
  L_WIFI_NOT_CONNECTED,
  L_SETUP_WIFI,
  L_BRIGHTNESS,
  L_TIME_FORMAT,
  L_TIME_12H,
  L_TIME_24H,
  L_SCREENSAVER,
  L_FLIP_CLOCK,
  L_MINIMAL,
  L_SCREEN_OFF,
  L_SCREEN_TIMEOUT,
  L_1_MIN,
  L_2_MIN,
  L_5_MIN,
  L_NEVER,
  L_RESET_PASS,
  L_LANGUAGE,
  L_HAPTIC,

  // --- Device Manager ---
  L_DEVICES,
  L_NEW_DEVICE,
  L_EDIT_DEVICE,
  L_DEVICE_NAME,
  L_DEVICE_NAME_HINT,
  L_STATE_TOPIC,
  L_STATE_TOPIC_HINT,
  L_CMD_TOPIC,
  L_CMD_TOPIC_HINT,
  L_DIMMER_TOPIC,
  L_DIMMER_TOPIC_HINT,
  L_ICON_TYPE,
  L_CONFIRM_DELETE,
  L_CONFIRM_DELETE_MSG,

  // --- Dimmer ---
  L_BRIGHTNESS_CTRL,
  L_TAP_OUTSIDE_CLOSE,

  // --- Scenes ---
  L_SCENES,
  L_NEW_SCENE,
  L_EDIT_SCENE,
  L_NO_SCENES,
  L_ACTIONS,
  L_SCENE_NAME,
  L_SCENE_NAME_HINT,
  L_SCENE_ICON,
  L_SCENE_ICONS,
  L_SCENE_ACTIONS,
  L_TOPIC,
  L_PAYLOAD,
  L_ACTION,
  L_TAP_TO_RUN,

  // --- Schedules ---
  L_SCHEDULES,
  L_SCHED,
  L_NO_SCHEDULES,
  L_EVERY_DAY,
  L_UNKNOWN,
  L_DAY_SU, L_DAY_MO, L_DAY_TU, L_DAY_WE, L_DAY_TH, L_DAY_FR, L_DAY_SA,

  // --- Screensaver ---
  L_SMART_HOME,

  // --- UI General ---
  L_ICON_NAMES,

  // --- Web Portal ---
  L_WEB_TITLE,
  L_WEB_SAVE_RESTART,
  L_WEB_NETWORK,
  L_WEB_DEVICE_MGMT,
  L_WEB_SYSTEM,
  L_WEB_MQTT_CONFIG,
  L_WEB_WIFI_CRED,
  L_WEB_MQTT_SERVER,
  L_WEB_WEATHER_CITY,
  L_WEB_PANEL_TITLE,
  L_WEB_TIMEZONE,
  L_WEB_AUTH,
  L_WEB_WALLPAPER,
  L_WEB_SS_TIMER,
  L_WEB_DEVICES,
  L_WEB_ADD_DEVICE,
  L_WEB_SEARCH,
  L_WEB_SCENES_TAB,
  L_WEB_SCHEDULES_TAB,
  L_WEB_ADD_SCENE,
  L_WEB_SAVE_SCENES,
  L_WEB_SCENE_HELP,
  L_WEB_ADD_SCHEDULE,
  L_WEB_SCHED_HELP,
  L_WEB_IP_ADDRESS,
  L_WEB_EXPORT_HELP,
  L_WEB_SAVED,
  L_WEB_LOAD_FAIL,
  L_WEB_MAX_DEVICES,
  L_WEB_CONFIRM_DEL,
  L_WEB_INVALID_MQTT,

  LANG_KEY_COUNT // must be last
};

// Get translated string by key. Returns English default if no override loaded.
const char *L(LangKey key);

// Load language file from LittleFS. Call after LittleFS.begin().
// path: e.g. "/lang_th.json"
void lang_load(const char *langCode);

// Get current language code (e.g. "en", "th")
const char *lang_current();

// Available language codes for settings dropdown
#define LANG_OPTIONS_COUNT 2
extern const char *lang_codes[LANG_OPTIONS_COUNT];     // {"en", "th"}
extern const char *lang_names[LANG_OPTIONS_COUNT];     // {"English", "ไทย"}
