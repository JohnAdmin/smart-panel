#pragma once
// Q3: compile-time constants ONLY.
// All runtime extern declarations live in include/globals.h
#include <Arduino.h>

// --- LGFX Hardware Configuration (ESP32-S3 SC01 Plus) ---
#define LGFX_BUS_FREQ_WRITE 20000000
#define LGFX_BUS_PIN_WR   47
#define LGFX_BUS_PIN_RD   -1
#define LGFX_BUS_PIN_RS   0
#define LGFX_BUS_PIN_D0   9
#define LGFX_BUS_PIN_D1   46
#define LGFX_BUS_PIN_D2   3
#define LGFX_BUS_PIN_D3   8
#define LGFX_BUS_PIN_D4   18
#define LGFX_BUS_PIN_D5   17
#define LGFX_BUS_PIN_D6   16
#define LGFX_BUS_PIN_D7   15
#define LGFX_PANEL_PIN_CS -1
#define LGFX_PANEL_PIN_RST 4
#define LGFX_PANEL_WIDTH  320
#define LGFX_PANEL_HEIGHT 480
#define LGFX_LIGHT_PIN_BL 45
#define LGFX_LIGHT_FREQ   44100
#define LGFX_LIGHT_CHANNEL 7
#define LGFX_TOUCH_I2C_PORT 1
#define LGFX_TOUCH_I2C_ADDR 0x38
#define LGFX_TOUCH_PIN_SDA  6
#define LGFX_TOUCH_PIN_SCL  5
#define LGFX_TOUCH_PIN_INT  7
#define LGFX_TOUCH_FREQ     400000

// --- System Configuration ---
#define MAX_DEVICES 100
#define DEFAULT_WIFI_SSID "Your_SSID"
#define DEFAULT_WIFI_PASS "Your_PASSWORD"
#define DEFAULT_MQTT_SERVER "192.168.1.140"
#define DEFAULT_MQTT_PORT 1883
#define DEFAULT_MQTT_USER ""
#define DEFAULT_MQTT_PASS ""

#define DEFAULT_WEB_USER "admin"
#define DEFAULT_WEB_PASS "admin"

#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 25200 // GMT+7
#define DAYLIGHT_OFFSET_SEC 0

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320

// Screensaver idle timeout (ms)
#define SCREENSAVER_TIMEOUT_MS 120000 // 2 minutes
#define WEATHER_UPDATE_MS 1800000     // 30 minutes
#define STOCK_UPDATE_MS   300000      // 5 minutes (Twelve Data free tier: 8 req/min)
#define STATUS_SYNC_ACTIVE_MS 30000   // 30 seconds re-subscribe for retained
#define STATUS_SYNC_SAVER_MS 300000 // 5 minutes when not using (screensaver ON)

// Timing constants
// Toggle debounce — ignore inbound state updates for this long after a tap.
// During this window, contrary state messages also trigger a forced re-assert
// of the user's intended state (see mqtt_manager.cpp), which breaks ON/OFF
// flapping caused by stale retained messages on the broker.
#define DEVICE_DEBOUNCE_MS    5000    // Toggle debounce + flap-suppression window
#define DEVICE_PENDING_GRACE_MS 1500  // Wait this long for a state echo before
                                      // a tile admits the command is unconfirmed
#define STALE_DEVICE_MS       300000  // 5 min — mark device stale if no MQTT
#define MQTT_BACKOFF_INIT_MS  5000    // Initial MQTT reconnect delay
#define MQTT_BACKOFF_MAX_MS   60000   // Maximum MQTT reconnect delay
#define DEVICE_STATE_SAVE_MS  10000   // Debounced save interval for dirty states
#define MQTT_HEARTBEAT_MS     30000   // Periodic heartbeat log interval
#define HTTP_TIMEOUT_MS       10000   // HTTP client timeout for weather API

// MQTT queue
#define MQTT_QUEUE_SIZE       16
#define MQTT_BUFFER_SIZE      1024

// The panel's own availability topic (LWT + retained heartbeat). A device
// entry pointing at this topic represents *this panel* to Homebridge, not
// something the user controls, so the dashboard hides it — see
// Device::isPanelStatus().
#define PANEL_STATUS_TOPIC    "sc01/status"

// MQTT debug: subscribe to wildcard "homebridge/#" to sniff all traffic.
// Useful when device topics don't match what Homebridge actually publishes.
// Set to 0 in production to reduce traffic.
#define MQTT_DEBUG_SNIFF_HOMEBRIDGE 1

// File paths (LittleFS)
#define FS_DEVICES_JSON    "/devices.json"
#define FS_DEVICES_BIN     "/devices.bin"
#define FS_WALLPAPER       "/wallpaper.jpg"
#define FS_ROOMS_JSON      "/rooms.json"
#define FS_SCENES_JSON     "/scenes.json"
#define FS_SCHEDULES_JSON  "/schedules.json"

// NVS namespace
#define NVS_NAMESPACE      "smartpanel"

// --- Haptic Feedback (Vibration Motor) ---
// Connect a small vibration motor via MOSFET to this GPIO.
// Set to -1 to disable at compile time.
#define HAPTIC_PIN          10
#define HAPTIC_BUZZ_MS      30    // vibration duration (ms)
#define HAPTIC_PWM_CHANNEL  6
#define HAPTIC_PWM_FREQ     1000  // Hz

// Weather defaults
#define DEFAULT_WEATHER_CITY "Bangkok"
#define DEFAULT_LATITUDE 13.75f
#define DEFAULT_LONGITUDE 100.52f

// --- Icon types — must match web portal ICON_OPTIONS order (0–9) ---
#define ICON_LAMP 0
#define ICON_FAN 1
#define ICON_SWITCH 2
#define ICON_PLUG 3
#define ICON_THERMOSTAT 4
#define ICON_LOCK 5
#define ICON_TV 6
#define ICON_GARAGE 7
#define ICON_STRIP 8
#define ICON_GENERIC 9

// Firmware version, shown on Settings -> System and in the portal.
#define FW_VERSION "1.5.0"

// --- Function declarations ---
// Wipes NVS and the LittleFS config files, then reboots. Extracted from the
// boot-time touch-and-hold path so Settings -> System can offer the same
// thing — holding the screen during boot is not something anyone discovers.
void factory_reset_now();

void loadSettings();
void applySettings(const char *ssid, const char *pass, const char *mqtt_server,
                   const char *mqtt_user, const char *mqtt_pass,
                   const char *city, const char *title, bool isDark,
                   bool isLargeTiles, int brightness, bool use24h,
                   const char *w_user, const char *w_pass, int gmtOffset = 7);
void loadDevices();
bool saveDevices();
bool addDevice(const char *name, const char *room, const char *stat,
               const char *cmnd, const char *dimmer, int icon, bool isFav, bool save = true);
void deleteDevice(int index);
const char *getIconSymbol(int icon_type);
void fetchWeather();
