// wifi_manager.cpp
// Contains ONLY WiFi/MQTT/OTA/NTP/Settings logic.
// Device CRUD  → device_store.cpp
// Weather      → weather.cpp
// Globals      → include/globals.h

#include "wifi_manager.h"
#include "config.h"
#include "globals.h"
#include "mqtt_manager.h"
#include "stock.h"
#include "ui.h"
#include "web_server.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <MQTT.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <esp_task_wdt.h>
#include <lvgl.h>
#include <mbedtls/base64.h>
#include <time.h>

// --- Module-scope singletons ---
WiFiClient espClient;
DNSServer dnsServer;
Preferences preferences;

// Captive-portal flag
bool isConfigMode = false;
const byte DNS_PORT = 53;

// --- Runtime state definitions (declared extern in globals.h) ---
volatile bool isWifiConnected = false;
volatile bool isMqttConnected = false;
bool timeSynced = false;

Device devices[MAX_DEVICES];
int deviceCount = 0;

char currentTime[10] = "00:00";
char currentDate[24] = "Mon, 01 Jan 2024";
char currentMeridiem[4] = "";

float weatherTemp = 0;
char weatherDesc[32] = "";
char weatherCity[32] = DEFAULT_WEATHER_CITY;
char weatherCityName[32] = DEFAULT_WEATHER_CITY;
bool weatherValid = false;
int  airQualityAqi = 0;
bool airQualityValid = false;

unsigned long lastTouchTime = 0;
bool screensaverActive = false;
int screensaverStyle = 0; // 0=Flip Clock, 1=Minimal, 2=Screen Off
unsigned long screensaverTimeoutMs = 120000; // default 2 minutes
volatile bool webActivityDetected = false;

char panelTitle[64] = "Hero Home Panel";
bool themeDark = true;
bool useLargeTiles = false;
int  homeLayoutStyle = 0; // 0=Modern, 1=Classic
bool use24HourFormat = true;
int gmtOffsetHours = 7; // default GMT+7 (Bangkok)
int wifiRssi = 0;       // WiFi RSSI in dBm
char currentLang[8] = "en"; // default language

String wifi_ssid = DEFAULT_WIFI_SSID;
String wifi_pass = DEFAULT_WIFI_PASS;
String mqtt_server_ip = DEFAULT_MQTT_SERVER;
int mqtt_port = DEFAULT_MQTT_PORT;
String mqtt_username = DEFAULT_MQTT_USER;
String mqtt_password = DEFAULT_MQTT_PASS;

String web_user = DEFAULT_WEB_USER;
String web_pass = DEFAULT_WEB_PASS;

int displayBrightness = 255;

// Timers for network_loop
unsigned long lastWifiCheck = 0;
unsigned long lastTimeUpdate = 0;
unsigned long lastWeatherUpdate = 0;
unsigned long lastStatusSync = 0;
static const unsigned long WIFI_RECONNECT_INTERVAL_MS = 15000;
static const unsigned long CONFIG_MODE_WIFI_RETRY_MS = 30000;
static unsigned long lastWifiRetry = 0;
static unsigned long configStaConnectSince = 0;
static const unsigned long CONFIG_MODE_STA_CONNECT_COOLDOWN_MS = 25000;
static volatile int lastWifiDisconnectReason = 0;

static bool hasConfiguredWifiCredentials() {
  String ssid = wifi_ssid;
  String pass = wifi_pass;
  ssid.trim();
  pass.trim();

  if (ssid.length() == 0 || ssid == DEFAULT_WIFI_SSID)
    return false;
  if (pass.length() == 0 || pass == DEFAULT_WIFI_PASS)
    return false;
  return true;
}

static void wifi_event_logger(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    lastWifiDisconnectReason = info.wifi_sta_disconnected.reason;
    Serial.printf("[WIFI] STA disconnected, reason=%d\n", lastWifiDisconnectReason);
  }
}

// --------------------------------------------------------
//  Credential Obfuscation
// --------------------------------------------------------
// Uses the ESP32 MAC address to apply a unique bitwise XOR cipher,
// encoding the result into Base64 so it can safely be stored in NVS.
String encrypt_password(const String &plain_text) {
  if (plain_text.length() == 0)
    return "";

  uint64_t mac = ESP.getEfuseMac();
  uint8_t *mac_bytes = (uint8_t *)&mac;

  String xor_str = "";
  for (size_t i = 0; i < plain_text.length(); i++) {
    xor_str += (char)(plain_text[i] ^ mac_bytes[i % 6]);
  }

  unsigned char out[256];
  size_t olen = 0;
  mbedtls_base64_encode(out, sizeof(out), &olen,
                        (const unsigned char *)xor_str.c_str(),
                        xor_str.length());

  String b64 = String((char *)out, olen);
  return "ENC:" + b64;
}

String decrypt_password(const String &stored_text) {
  if (stored_text.length() == 0)
    return "";

  // Legacy plaintext fallback
  if (!stored_text.startsWith("ENC:")) {
    return stored_text;
  }

  String b64 = stored_text.substring(4); // Strip "ENC:"

  unsigned char out[256];
  size_t olen = 0;
  mbedtls_base64_decode(out, sizeof(out), &olen,
                        (const unsigned char *)b64.c_str(), b64.length());

  String xor_str = String((char *)out, olen);

  uint64_t mac = ESP.getEfuseMac();
  uint8_t *mac_bytes = (uint8_t *)&mac;

  String plain_text = "";
  for (size_t i = 0; i < xor_str.length(); i++) {
    plain_text += (char)(xor_str[i] ^ mac_bytes[i % 6]);
  }

  return plain_text;
}

// --------------------------------------------------------
//  Settings — NVS load/save
// --------------------------------------------------------
void loadSettings() {
  if (!preferences.begin(NVS_NAMESPACE, true)) {
    Serial.println("[NVS] WARNING: Failed to initialize preferences for read! "
                   "Using defaults.");
  }
  String stored_ssid = preferences.getString("ssid", DEFAULT_WIFI_SSID);
  wifi_ssid = stored_ssid; // Usually not considered highly sensitive, but could
                           // be encrypted if desired

  String stored_pass = preferences.getString("pass", DEFAULT_WIFI_PASS);
  wifi_pass = decrypt_password(stored_pass);
  Serial.printf("[NVS] WiFi loaded: SSID='%s' (len=%u), PASS len=%u\n",
                wifi_ssid.c_str(), (unsigned)wifi_ssid.length(),
                (unsigned)wifi_pass.length());

  mqtt_server_ip = preferences.getString("mqtt_srv", DEFAULT_MQTT_SERVER);
  mqtt_port = preferences.getInt("mqtt_port", DEFAULT_MQTT_PORT);
  mqtt_username = preferences.getString("mqtt_usr", DEFAULT_MQTT_USER);

  String stored_mqtt_pwd = preferences.getString("mqtt_pwd", DEFAULT_MQTT_PASS);
  mqtt_password = decrypt_password(stored_mqtt_pwd);

  web_user = preferences.getString("web_user", DEFAULT_WEB_USER);

  String stored_web_pass = preferences.getString("web_pass", DEFAULT_WEB_PASS);
  if (stored_web_pass == DEFAULT_WEB_PASS) {
    web_pass = DEFAULT_WEB_PASS;
  } else {
    web_pass = decrypt_password(stored_web_pass);
  }

  String city = preferences.getString("weather_city", DEFAULT_WEATHER_CITY);
  strncpy(weatherCity, city.c_str(), sizeof(weatherCity) - 1);
  weatherCity[sizeof(weatherCity) - 1] = '\0';

  strncpy(weatherCityName, weatherCity, sizeof(weatherCityName) - 1);
  weatherCityName[sizeof(weatherCityName) - 1] = '\0';

  String title = preferences.getString("panel_title", "Hero Home Panel");
  strncpy(panelTitle, title.c_str(), sizeof(panelTitle) - 1);
  panelTitle[sizeof(panelTitle) - 1] = '\0';

  themeDark = preferences.getBool("theme_dark", true);
  useLargeTiles = preferences.getBool("large_tiles", false);
  homeLayoutStyle = preferences.getInt("home_layout", 0);
  use24HourFormat = preferences.getBool("time_24h", true);
  displayBrightness = preferences.getInt("brightness", 255);
  if (displayBrightness < 10) {
    displayBrightness = 120;
    Serial.println("[SETTINGS] Brightness was too low, clamped to 120");
  }
  screensaverStyle = preferences.getInt("ss_style", 0);
  screensaverTimeoutMs =
      (unsigned long)preferences.getULong("ss_timeout", 120000);
  gmtOffsetHours = preferences.getInt("gmt_offset", 7);
  String lang = preferences.getString("lang", "en");
  strncpy(currentLang, lang.c_str(), sizeof(currentLang) - 1);
  currentLang[sizeof(currentLang) - 1] = '\0';
  hapticEnabled = preferences.getBool("haptic", true);
  preferences.end();

  // Load stock ticker config (separate NVS keys, same namespace)
  loadStockConfig();
}

void applySettings(const char *ssid, const char *pass, const char *mqtt_server,
                   const char *mqtt_user, const char *mqtt_pass,
                   const char *city, const char *title, bool isDark,
                   bool isLargeTiles, int brightness, bool use24h,
                   const char *w_user, const char *w_pass, int gmtOffset) {
  // This function always ends in a reboot. That guarantees every change
  // (WiFi, MQTT, theme, language, etc.) takes effect cleanly, without the
  // edge cases where "no change detected" leaves the user stuck — e.g.
  // re-saving the same WiFi credentials while the connection is down.

  int safeBrightness = constrain(brightness, 10, 255);

  if (!preferences.begin(NVS_NAMESPACE, false)) {
    Serial.println("[NVS] CRITICAL: Failed to open preferences for writing!");
  }
  preferences.putString("ssid", ssid);
  // Store WiFi password as plaintext for reliability. Some encrypted values
  // can decode differently across save/reboot cycles and break reconnect.
  // decrypt_password() already supports legacy plaintext fallback.
  preferences.putString("pass", String(pass));
  preferences.putString("mqtt_srv", mqtt_server);
  preferences.putInt("mqtt_port", mqtt_port);
  preferences.putString("mqtt_usr", mqtt_user);
  preferences.putString("mqtt_pwd", encrypt_password(String(mqtt_pass)));
  preferences.putString("weather_city", city);
  preferences.putString("panel_title", title);
  preferences.putBool("theme_dark", isDark);
  preferences.putBool("large_tiles", isLargeTiles);
  preferences.putBool("time_24h", use24h);
  preferences.putInt("brightness", safeBrightness);
  preferences.putString("web_user", w_user);
  preferences.putString("web_pass", encrypt_password(String(w_pass)));
  preferences.putInt("gmt_offset", gmtOffset);
  preferences.putString("lang", currentLang);
  preferences.end();

  gmtOffsetHours = gmtOffset;

  // Apply non-network settings immediately without reboot
  displayBrightness = safeBrightness;
  strncpy(panelTitle, title, sizeof(panelTitle) - 1);
  panelTitle[sizeof(panelTitle) - 1] = '\0';
  themeDark = isDark;
  useLargeTiles = isLargeTiles;
  use24HourFormat = use24h;

  Serial.println("[SETTINGS] Save complete — pending reboot flag set");
  pending_ota_reboot = true;
  ota_reboot_time = millis();
}

// --------------------------------------------------------
//  network_load_persistence
// --------------------------------------------------------
void network_load_persistence() {
  loadSettings();

  // Mount LittleFS for wallpaper storage
  if (!LittleFS.begin(true)) {
    Serial.println("[FS] LittleFS mount failed!");
  } else {
    Serial.println("[FS] LittleFS mounted OK (V1)");
  }
  loadDevices();

  // No default devices — user adds via web portal
  if (deviceCount == 0) {
    Serial.println("[DEV] No devices found. Add devices via web portal.");
  }
}

void timeSyncCallback(struct timeval *tv) {
  Serial.println("NTP Time Sync Successful!");
  timeSynced = true;
}

// --------------------------------------------------------
//  network_setup
// --------------------------------------------------------
void network_setup() {
  lastTouchTime = millis();

  static bool wifiEventRegistered = false;
  if (!wifiEventRegistered) {
    WiFi.onEvent(wifi_event_logger);
    wifiEventRegistered = true;
  }

  bool canTrySta = hasConfiguredWifiCredentials();
  if (canTrySta) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setHostname("SC01-Plus-Panel");
    WiFi.disconnect(false, false);
    delay(100);
    WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  } else {
    Serial.println("[NETWORK] WiFi credentials not configured yet. Starting AP setup mode.");
  }

  Serial.print("Connecting to WiFi");
  uint32_t wifiWaitStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiWaitStart < 15000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (canTrySta && WiFi.status() != WL_CONNECTED) {
    Serial.println("[NETWORK] First WiFi attempt failed, retrying with STA reset...");
    WiFi.disconnect(true, false);
    delay(200);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setHostname("SC01-Plus-Panel");
    WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());

    Serial.print("Retrying WiFi");
    uint32_t retryStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - retryStart < 10000) {
      delay(250);
      Serial.print(".");
    }
    Serial.println();
  }

  if (canTrySta && WiFi.status() != WL_CONNECTED) {
    Serial.printf("[NETWORK] WiFi still not connected (last reason=%d)\n",
                  lastWifiDisconnectReason);
  }

  if (WiFi.status() == WL_CONNECTED) {
    isWifiConnected = true;
    sntp_set_time_sync_notification_cb(timeSyncCallback);
    configTime((long)gmtOffsetHours * 3600L, DAYLIGHT_OFFSET_SEC, NTP_SERVER,
               "time.nist.gov", "time.google.com");

    mqtt_manager_setup();
    ArduinoOTA.setHostname("SC01-Plus-Panel");
    ArduinoOTA.begin();
    
    // Initialize mDNS
    if (MDNS.begin("smartpanel")) {
      Serial.println("mDNS responder started: http://smartpanel.local");
      MDNS.addService("http", "tcp", 80);
    } else {
      Serial.println("Error setting up mDNS responder!");
    }

    web_server_init();
    Serial.printf("Network up. MQTT: %s Devices: %d\n", mqtt_server_ip.c_str(),
                  deviceCount);
  } else {
    Serial.println("WiFi Failed. Starting Captive Portal AP...");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("SC01-Plus-Setup");
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    lastWifiRetry = millis();
    configStaConnectSince = 0;
    web_server_init();
    isConfigMode = true;
    Serial.printf("AP started: %s\n", WiFi.softAPIP().toString().c_str());
    lv_disp_trig_activity(NULL); // Reset LVGL idle timer so screensaver doesn't activate immediately
  }
}

// --------------------------------------------------------
//  network_loop
// --------------------------------------------------------
void network_loop() {
  if (isConfigMode) {
    dnsServer.processNextRequest();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("[NETWORK] WiFi recovered in config mode, switching back to STA...");
      WiFi.softAPdisconnect(true);
      isConfigMode = false;
      configStaConnectSince = 0;
      WiFi.mode(WIFI_STA);
      WiFi.setAutoReconnect(true);

      isWifiConnected = true;
      sntp_set_time_sync_notification_cb(timeSyncCallback);
      configTime((long)gmtOffsetHours * 3600L, DAYLIGHT_OFFSET_SEC, NTP_SERVER,
                 "time.nist.gov", "time.google.com");
      mqtt_manager_setup();
      ArduinoOTA.begin();
      if (MDNS.begin("smartpanel")) {
        Serial.println("mDNS responder started: http://smartpanel.local");
        MDNS.addService("http", "tcp", 80);
      }
      ui_update_header();
      return;
    }

    if (millis() - lastWifiRetry > CONFIG_MODE_WIFI_RETRY_MS) {
      lastWifiRetry = millis();

      if (!hasConfiguredWifiCredentials()) {
        Serial.println("[NETWORK] Retry skipped: WiFi credentials not configured yet");
        return;
      }

      if (configStaConnectSince != 0 &&
          millis() - configStaConnectSince < CONFIG_MODE_STA_CONNECT_COOLDOWN_MS) {
        Serial.println("[NETWORK] Retry skipped: previous STA connect still in cooldown");
        return;
      }

      wl_status_t st = WiFi.status();
      if (st == WL_IDLE_STATUS) {
        Serial.println("[NETWORK] Retry skipped: STA is still connecting...");
      } else if (st == WL_CONNECTED) {
        Serial.println("[NETWORK] Retry skipped: already connected");
      } else if (st == WL_DISCONNECTED || st == WL_CONNECTION_LOST ||
                 st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL) {
        Serial.printf("[NETWORK] Config-mode reconnect, WiFi status=%d\n", (int)st);
        // Hard reset WiFi state machine to recover from stuck STA states.
        WiFi.mode(WIFI_OFF);
        delay(150);
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP("SC01-Plus-Setup");
        WiFi.setAutoReconnect(true);
        WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
        configStaConnectSince = millis();
      } else {
        Serial.printf("[NETWORK] Retry skipped: unsupported WiFi status=%d\n", (int)st);
      }
    }
    return;
  }

  // NOTE: Time/date update (every second) is handled exclusively in main.cpp
  // loop() to avoid two concurrent writers to currentTime[] and currentDate[].
  // network_loop() handles WiFi reconnect, OTA polling, and MQTT keepalive.

  if (WiFi.status() == WL_CONNECTED) {
    wifiRssi = WiFi.RSSI(); // Update signal strength
    if (!isWifiConnected) {
      isWifiConnected = true;
      ui_update_header();
      fetchWeather();
      lastWeatherUpdate = millis();
      if (stockEnabled) {
        fetchStocks();
        lastStockUpdate = millis();
      }
    }

    ArduinoOTA.handle();

    // Retry every 30s while invalid, otherwise normal 30-minute refresh
    uint32_t weatherInterval = weatherValid ? WEATHER_UPDATE_MS : 30000;
    if (millis() - lastWeatherUpdate > weatherInterval) {
      lastWeatherUpdate = millis();
      fetchWeather();
    }

    mqtt_manager_loop();

    // Stock ticker update (every 5 minutes)
    if (stockEnabled && millis() - lastStockUpdate > STOCK_UPDATE_MS) {
      lastStockUpdate = millis();
      fetchStocks();
    }
  } else {
    if (isWifiConnected) {
      isWifiConnected = false;
      isMqttConnected = false;
      ui_update_header();
    }

    if (millis() - lastWifiRetry > WIFI_RECONNECT_INTERVAL_MS) {
      lastWifiRetry = millis();
      Serial.println("[NETWORK] WiFi disconnected, attempting reconnect...");
      WiFi.reconnect();
    }
  }
}
