#pragma once
// globals.h
// Q3: Single header that declares ALL mutable global state externs.
// Include this instead of sprinkling extern declarations across .cpp files.
// config.h is now constants-only; this file owns the runtime state externs.

#include "config.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t lvgl_mux;
extern SemaphoreHandle_t devices_mux; // protects devices[] and mqttClient across cores

// --- Network State ---
extern volatile bool isWifiConnected;
extern volatile bool isMqttConnected;
extern bool isConfigMode;

#include <MQTT.h>
extern MQTTClient mqttClient;

// --- Device State ---
#include "device.h"
extern Device devices[MAX_DEVICES];
extern int deviceCount;

// --- Room State (rooms[] / roomCount declared in room.h) ---
#include "room.h"

// --- Settings (mutable runtime values) ---
extern String wifi_ssid;
extern String wifi_pass;
extern String mqtt_server_ip;
extern int mqtt_port;
extern String mqtt_username;
extern String mqtt_password;
extern int displayBrightness;
extern String web_user;
extern String web_pass;

// --- Time ---
extern char currentTime[10];
extern char currentDate[24];
// "AM" / "PM" in 12-hour mode, empty in 24-hour mode. Kept out of currentTime
// so that stays exactly "HH:MM" for the flip clock's per-digit extraction.
extern char currentMeridiem[4];

// --- Weather ---
extern float weatherTemp;
// WMO weather code. The description is looked up from it at render time
// rather than stored: a Thai condition can run to 42 bytes, which never fit
// the old char[32], and translating late means a language switch shows up
// immediately instead of after the next 30-minute fetch.
extern int weatherCode;
extern char weatherCity[32];
extern char weatherCityName[32];
extern bool weatherValid;
// Coordinates chosen in the portal's city picker. Zero means "never picked" —
// fetchWeather() then geocodes the city name the way it always did.
extern float weatherLat;
extern float weatherLon;

// --- Air quality (Open-Meteo, same coordinates as the weather fetch) ---
extern int  airQualityAqi;   // US AQI, 0-500
extern bool airQualityValid;

// --- Stock Ticker ---
#include "../src/stock.h"

// --- UI / UX ---
extern char panelTitle[64];
extern bool themeDark;
extern bool useLargeTiles;
extern int  homeLayoutStyle; // 0=Modern (weather+favs), 1=Classic (full-width grid)
extern bool use24HourFormat;
extern unsigned long lastTouchTime;
extern bool screensaverActive;
extern int screensaverStyle; // 0=Flip Clock, 1=Minimal, 2=Screen Off
extern unsigned long screensaverTimeoutMs; // user-configurable idle timeout
extern volatile bool webActivityDetected; // set by web callbacks, consumed by main loop
extern int gmtOffsetHours; // timezone offset in hours (e.g. 7 for GMT+7)
extern int wifiRssi;       // WiFi signal strength in dBm (updated periodically)
extern char currentLang[8]; // language code: "en", "th", etc.
extern bool hapticEnabled;  // haptic feedback on/off (persisted in NVS)

// --- Network loop timers (defined in wifi_manager.cpp) ---
extern unsigned long lastTimeUpdate; // drives 1-second UI clock in main.cpp
extern unsigned long lastWeatherUpdate;
extern unsigned long lastStatusSync;

// --- Device state persistence ---
extern volatile bool deviceStatesDirty;

// --- Global NVS preferences object ---
// Defined once in wifi_manager.cpp, used in device_store.cpp and
// ui_settings.cpp
#include <Preferences.h>
extern Preferences preferences;

// --- Safe Watchdog Wrapper ---
void safe_wdt_reset();

// --- OTA progress (written by the /api/update upload handler on Core 0) ---
extern volatile bool otaActive;
extern volatile int  otaProgressPct;

// --- OTA Reboot Flag ---
extern bool pending_ota_reboot;
extern unsigned long ota_reboot_time;
