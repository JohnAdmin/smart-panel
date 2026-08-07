#include "web_server.h"
#include <lvgl.h>
#include "config.h"
#include "config_json.h"
#include "globals.h"

// Forward declaration — implemented in ui/ui_screensaver.cpp
void invalidate_screensaver_build();
#include "lang.h"
#include "scene.h"
#include "schedule.h"
#include "web_page.h"
#include "wifi_manager.h"
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <Update.h>
#include <esp_heap_caps.h>

// PSRAM allocator for ArduinoJson 7. Routes JsonDocument storage to
// external SPIRAM so building large JSON payloads does not exhaust the
// limited internal heap (which on this build idles around ~11 KB free).
struct PsramAllocator : ArduinoJson::Allocator {
  void *allocate(size_t size) override {
    void *p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = malloc(size); // fallback to internal heap
    return p;
  }
  void deallocate(void *p) override { free(p); }
  void *reallocate(void *p, size_t size) override {
    void *np = heap_caps_realloc(p, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!np) np = realloc(p, size);
    return np;
  }
};
static PsramAllocator psramAllocator;

// Define the AsyncWebServer on port 80
AsyncWebServer server(80);

// ========================================================
//  ROUTE HELPERS
// ========================================================

// Credential check for a request handler. On failure the 401 challenge is
// sent here, so callers only need `if (!web_require_auth(req)) return;`.
static bool web_require_auth(AsyncWebServerRequest *request) {
  if (request->authenticate(web_user.c_str(), web_pass.c_str()))
    return true;
  request->requestAuthentication();
  return false;
}

// Credential check without sending a challenge. Body and upload callbacks use
// this: the paired request handler has already issued the 401, and answering
// twice on one request corrupts the response.
static bool web_authed(AsyncWebServerRequest *request) {
  return request->authenticate(web_user.c_str(), web_pass.c_str());
}

// Allocate from PSRAM, falling back to internal heap. The panel idles with
// only ~11 KB of internal heap free, so request bodies must not land there
// while PSRAM is available.
static void *web_alloc(size_t bytes) {
#ifdef BOARD_HAS_PSRAM
  void *p = ps_malloc(bytes);
  if (p)
    return p;
#endif
  return malloc(bytes);
}

// Accumulates a chunked request body, then hands the parsed JSON to
// `onComplete`. Chunks arrive one per call on the async server task, so the
// scratch buffer is parked on request->_tempObject until the last one lands.
//
// The request is answered here and `onComplete` is skipped on overflow, OOM,
// or malformed JSON. Freeing the buffer before `onComplete` runs is safe:
// deserializeJson() is given a `const char *`, so ArduinoJson copies every
// string into the document rather than aliasing the buffer.
template <typename Fn>
static void web_collect_json_body(AsyncWebServerRequest *request, uint8_t *data,
                                  size_t len, size_t index, size_t total,
                                  size_t maxBytes, const char *tag,
                                  Fn onComplete) {
  if (index == 0) {
    if (total > maxBytes) {
      Serial.printf("[WEB] %s: payload %u B exceeds %u B cap\n", tag,
                    (unsigned)total, (unsigned)maxBytes);
      request->send(413, "text/plain", "Payload Too Large");
      return;
    }
    request->_tempObject = web_alloc(total + 1);
    if (!request->_tempObject) {
      Serial.printf("[WEB] %s: allocation failed for %u B\n", tag,
                    (unsigned)total);
      request->send(500, "text/plain", "Server OOM");
      return;
    }
  }

  // Earlier chunk already failed and answered the request — drop the rest.
  if (!request->_tempObject)
    return;

  memcpy((uint8_t *)request->_tempObject + index, data, len);
  if (index + len != total)
    return;

  ((uint8_t *)request->_tempObject)[total] = '\0';

  JsonDocument doc(&psramAllocator);
  DeserializationError error =
      deserializeJson(doc, (const char *)request->_tempObject);
  free(request->_tempObject);
  request->_tempObject = NULL;

  if (error) {
    Serial.printf("[WEB] %s: JSON error: %s\n", tag, error.c_str());
    request->send(400, "text/plain", "Invalid JSON");
    return;
  }

  Serial.printf("[WEB] %s: %u B body parsed, heap=%u\n", tag, (unsigned)total,
                ESP.getFreeHeap());
  onComplete(doc);
}

// ========================================================
//  SERVER INITIALIZATION
// ========================================================
void web_server_init() {

  // 1. Serve the Frontend HTML
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    webActivityDetected = true;
    Serial.printf("[WEB] GET / from %s, heap=%u\n", request->client()->remoteIP().toString().c_str(), ESP.getFreeHeap());
    if (!web_require_auth(request)) {
      Serial.println("[WEB] / -> 401 auth required");
      return;
    }
    AsyncWebServerResponse *response =
        request->beginResponse(200, "text/html; charset=UTF-8", index_html);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "-1");
    request->send(response);
    Serial.printf("[WEB] / sent, heap_after=%u\n", ESP.getFreeHeap());
  });

  // Ignore favicon to prevent 500 errors in browser console
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(204);
  });

  // Serve the wallpaper image
  server.on("/wallpaper.jpg", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (LittleFS.exists("/wallpaper.jpg")) {
      request->send(LittleFS, "/wallpaper.jpg", "image/jpeg");
    } else {
      request->send(404, "text/plain", "Not Found");
    }
  });

  // Serve default wallpapers — no auth needed (public presets), cached by browser
  static const char *const kWallpaperPresets[] = {
      "/default1.jpg", "/default2.jpg", "/default3.jpg"};
  for (const char *preset : kWallpaperPresets) {
    server.on(preset, HTTP_GET, [preset](AsyncWebServerRequest *request) {
      if (!LittleFS.exists(preset)) {
        request->send(404, "text/plain", "Not Found");
        return;
      }
      AsyncWebServerResponse *r =
          request->beginResponse(LittleFS, preset, "image/jpeg");
      r->addHeader("Cache-Control", "public, max-age=86400");
      request->send(r);
    });
  }

  // 2. API endpoint to fetch current configuration
  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    webActivityDetected = true;
    Serial.printf("[WEB] GET /api/config, heap=%u, devCount=%d\n", ESP.getFreeHeap(), deviceCount);
    if (!web_require_auth(request)) {
      Serial.println("[WEB] /api/config -> 401");
      return;
    }
    // Stream JSON directly to client to avoid double-buffering
    // (JsonDocument + String) which exhausts heap on this MCU.
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "-1");

    JsonDocument doc(&psramAllocator);
    doc["wifi_ssid"] = wifi_ssid;
    doc["wifi_pass"] = "********";
    doc["mqtt_srv"] = mqtt_server_ip;
    doc["mqtt_port"] = mqtt_port;
    doc["mqtt_usr"] = mqtt_username;
    doc["mqtt_pwd"] = "********";
    doc["weather_city"] = weatherCity;
    // Zero means the city was typed rather than picked, and the panel will
    // geocode it. The portal uses that to decide whether to show "pinned".
    doc["weather_lat"] = weatherLat;
    doc["weather_lon"] = weatherLon;
    doc["panel_title"] = panelTitle;
    doc["theme_dark"] = themeDark;
    doc["large_tiles"] = useLargeTiles;
    doc["time_24h"] = use24HourFormat;
    doc["brightness"] = displayBrightness;
    doc["gmt_offset"] = gmtOffsetHours;
    doc["web_user"] = web_user;
    doc["web_pass"] = "********";
    doc["ss_timeout"] = (int)(screensaverTimeoutMs / 1000);
    doc["lang"] = lang_current();

    JsonArray devs = doc["devices"].to<JsonArray>();
    for (int i = 0; i < deviceCount; i++) {
      JsonObject dev = devs.add<JsonObject>();
      dev["name"] = devices[i].name;
      dev["room"] = devices[i].room;
      dev["state_topic"] = devices[i].state_topic;
      dev["cmnd_topic"] = devices[i].cmnd_topic;
      dev["dimmer_topic"] = devices[i].dimmer_topic;
      dev["dev_type"] = devices[i].dev_type;
      dev["icon_type"] = devices[i].icon_type;
      dev["is_favorite"] = devices[i].is_favorite;
    }

    size_t written = serializeJson(doc, *response);
    Serial.printf("[WEB] /api/config -> %u bytes streamed, heap_after=%u\n", written, ESP.getFreeHeap());
    request->send(response);
  });

  // 3. API endpoint to receive and save new configuration
  server.on(
      "/api/save", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        if (!web_require_auth(request))
          return;
      },
      NULL,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len,
         size_t index, size_t total) {
        if (!web_authed(request))
          return;

        if (index == 0) {
          webActivityDetected = true;
          Serial.printf("[WEB] Save started. Total Body: %u bytes, Free Heap: %u\n", total, ESP.getFreeHeap());
        }

        // Safety cap: no reason for a settings payload to exceed 128 KB.
        web_collect_json_body(
            request, data, len, index, total, 128000, "/api/save",
            [request](JsonDocument &doc) {
          // Save Devices FIRST (Batch-optimized)
          // Safety: only rebuild list if a NON-EMPTY array was provided.
          // Empty/missing devices field must NOT wipe persisted devices,
          // otherwise reboot loses everything when web posts settings-only payloads.
          if (doc["devices"].is<JsonArray>() && doc["devices"].as<JsonArray>().size() > 0) {
            JsonArray devs = doc["devices"];
            Serial.printf("[WEB] Batch updating %d devices\n", devs.size());

            // Clear memory first (mutex-protected)
            if (xSemaphoreTake(devices_mux, pdMS_TO_TICKS(500)) != pdTRUE) {
              request->send(503, "text/plain", "Device lock busy");
              return;
            }
            deviceCount = 0;
            bool success = true;

            for (JsonObject dev : devs) {
              String n = dev["name"] | "Unknown";
              String r = dev["room"] | "Living Room";
              String s = dev["state_topic"] | "";
              String c = dev["cmnd_topic"] | "";
              String d = dev["dimmer_topic"] | "";
              n.trim();
              r.trim();
              s.trim();
              c.trim();
              d.trim();
              if (!addDevice(n.c_str(), r.c_str(), s.c_str(), c.c_str(),
                             d.c_str(), dev["icon_type"] | 0,
                             dev["is_favorite"] | false, false)) {
                success = false;
                break;
              }
              // Control type. A payload from an older portal build has no
              // dev_type, so fall back to the same rule loadDevices() uses:
              // a level topic means a dimmer, anything else a plain switch.
              {
                Device &nd = devices[deviceCount - 1];
                int t = dev["dev_type"] | -1;
                nd.dev_type = (t >= 0 && t < DEV_TYPE_COUNT)
                                  ? (uint8_t)t
                                  : (nd.dimmer_topic[0] ? DEV_DIMMER : DEV_TOGGLE);
              }
            }

            if (!success || !saveDevices()) {
              xSemaphoreGive(devices_mux);
              request->send(507, "text/plain", "Storage Error");
              return;
            }
            xSemaphoreGive(devices_mux);
          }

          // Save Network/Panel settings and restart
          if (doc["wifi_ssid"].is<const char *>() &&
              doc["wifi_pass"].is<const char *>() &&
              doc["mqtt_srv"].is<const char *>()) {

            // Preserve existing secrets when the masked placeholder OR an
            // empty value is sent back by the portal.
            const char *save_wifi_pass = doc["wifi_pass"] | "";
            if (strcmp(save_wifi_pass, "********") == 0 || strlen(save_wifi_pass) == 0) {
              save_wifi_pass = wifi_pass.c_str();
            }
            const char *save_mqtt_pwd = doc["mqtt_pwd"] | "";
            if (strcmp(save_mqtt_pwd, "********") == 0 || strlen(save_mqtt_pwd) == 0) {
              save_mqtt_pwd = mqtt_password.c_str();
            }
            const char *save_web_pass = doc["web_pass"] | "admin";
            if (strcmp(save_web_pass, "********") == 0 || strlen(save_web_pass) == 0) {
              save_web_pass = web_pass.c_str();
            }

            String save_wifi_ssid = doc["wifi_ssid"] | "";
            save_wifi_ssid.trim();
            // Preserve existing SSID if frontend sent empty (avoid wiping wifi by mistake)
            if (save_wifi_ssid.length() == 0) save_wifi_ssid = wifi_ssid;
            String save_wifi_pass_str = save_wifi_pass;
            save_wifi_pass_str.trim();

            bool hasRealSsid = save_wifi_ssid.length() > 0 &&
                               save_wifi_ssid != DEFAULT_WIFI_SSID;
            bool hasRealPass = save_wifi_pass_str.length() > 0 &&
                               save_wifi_pass_str != DEFAULT_WIFI_PASS;
            if (hasRealSsid && !hasRealPass) {
              request->send(400, "text/plain",
                            "WiFi password is required before reconnect/restart");
              return;
            }

            String save_mqtt_srv = doc["mqtt_srv"] | "";
            save_mqtt_srv.trim();
            // Preserve existing MQTT server if frontend sent empty
            if (save_mqtt_srv.length() == 0) save_mqtt_srv = mqtt_server_ip;
            String save_mqtt_user = doc["mqtt_usr"] | "";
            save_mqtt_user.trim();
            // Preserve existing MQTT user if frontend sent empty
            if (save_mqtt_user.length() == 0) save_mqtt_user = mqtt_username;
            String save_web_user = doc["web_user"] | "admin";
            save_web_user.trim();

            applySettings(save_wifi_ssid.c_str(), save_wifi_pass_str.c_str(),
                          save_mqtt_srv.c_str(), save_mqtt_user.c_str(),
                          save_mqtt_pwd, doc["weather_city"] | "",
                          doc["panel_title"] | "Hero Home Panel",
                          doc["theme_dark"] | true, doc["large_tiles"] | false,
                          doc["brightness"] | 255, doc["time_24h"] | true,
                          save_web_user.c_str(), save_web_pass,
                          doc["gmt_offset"] | 7);
            mqtt_port = doc["mqtt_port"] | DEFAULT_MQTT_PORT;
            preferences.begin(NVS_NAMESPACE, false);
            preferences.putInt("mqtt_port", mqtt_port);

            // City coordinates. The portal sends 0/0 when the field was typed
            // rather than picked from the list, which clears any earlier pin
            // and puts the panel back on geocoding — otherwise editing the
            // city by hand would leave it fetching for the previous place.
            // Absent is not the same as zero. A portal page older than the
            // coordinate fields sends no weather_lat at all, and treating that
            // as "clear the pin" silently wiped a pinned location every time
            // such a tab hit Save. Only an explicit value changes anything;
            // the portal sends 0 deliberately when the field is emptied.
            if (doc["weather_lat"].is<float>() || doc["weather_lon"].is<float>()) {
              float w_lat = doc["weather_lat"] | 0.0f;
              float w_lon = doc["weather_lon"] | 0.0f;
              if (w_lat < -90.0f || w_lat > 90.0f || w_lon < -180.0f ||
                  w_lon > 180.0f) {
                w_lat = w_lon = 0.0f;
              }
              weatherLat = w_lat;
              weatherLon = w_lon;
              preferences.putFloat("w_lat", w_lat);
              preferences.putFloat("w_lon", w_lon);
            } else {
              Serial.println("[WEB] No coordinates in payload — keeping stored pin");
            }
            preferences.end();
            request->send(200, "text/plain", "OK");
            Serial.println("[WEB] Settings saved — restarting now...");
            Serial.flush();
            delay(300);
            ESP.restart();
          } else {
            request->send(200, "text/plain", "OK");
            pending_ota_reboot = true;
            ota_reboot_time = millis();
            Serial.println("[WEB] Generic save path — restarting now...");
            Serial.flush();
            delay(300);
            ESP.restart();
          }
        });
      });

  // 4. Wallpaper status
  server.on("/api/wallpaper-status", HTTP_GET,
            [](AsyncWebServerRequest *request) {
              if (!web_require_auth(request))
                return;
              JsonDocument doc(&psramAllocator);
              bool exists = LittleFS.exists("/wallpaper.jpg");
              doc["exists"] = exists;
              if (exists) {
                File f = LittleFS.open("/wallpaper.jpg", "r");
                doc["size"] = f ? (int)f.size() : 0;
                if (f)
                  f.close();
              }
              String resp;
              serializeJson(doc, resp);
              request->send(200, "application/json", resp);
            });

  // 5. Wallpaper upload (multipart)
  server.on(
      "/api/wallpaper-upload", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        if (!web_require_auth(request))
          return;
        // Send OK, then restart to apply wallpaper
        request->send(200, "text/plain", "RESTART");
        delay(800);
        ESP.restart();
      },
      [](AsyncWebServerRequest *request, const String &filename, size_t index,
         uint8_t *data, size_t len, bool final) {
        if (!web_authed(request))
          return;

        if (!index) {
          Serial.printf("[WEB] Wallpaper Upload Start: %s\n", filename.c_str());
          request->_tempFile = LittleFS.open("/wallpaper.jpg", "w");
        }
        if (request->_tempFile) {
          request->_tempFile.write(data, len);
        }
        if (final) {
          if (request->_tempFile)
            request->_tempFile.close();
          Serial.printf("[WEB] Wallpaper Upload Complete: %s, %u B\n",
                        filename.c_str(), index + len);
        }
      });

  // 6. Web OTA Update (multipart)
  server.on(
      "/api/update", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        if (!web_require_auth(request))
          return;
        bool shouldReboot = !Update.hasError();
        AsyncWebServerResponse *response = request->beginResponse(
            200, "text/plain", shouldReboot ? "OK" : "FAIL");
        response->addHeader("Connection", "close");
        request->send(response);
        
        if (shouldReboot) {
          pending_ota_reboot = true;
          ota_reboot_time = millis();
        }
      },
      [](AsyncWebServerRequest *request, const String &filename, size_t index,
         uint8_t *data, size_t len, bool final) {
        if (!web_authed(request))
          return;

        if (!index) {
          Serial.printf("[OTA] Update Start: %s\n", filename.c_str());
          // If we don't know the size, UPDATE_SIZE_UNKNOWN is used.
          // Command is U_FLASH for firmware.
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            Update.printError(Serial);
          }
          // Surfaced on Settings → System, so an upload started from a laptop
          // is visible on the panel being replaced.
          otaActive = true;
          otaProgressPct = 0;
        }

        if (!Update.hasError()) {
          if (Update.write(data, len) != len) {
            Update.printError(Serial);
          }
        }

        // Update.begin() ran with UPDATE_SIZE_UNKNOWN, so the request's own
        // content length is the only total available.
        {
          const size_t total_len = request->contentLength();
          if (total_len > 0) {
            int pct = (int)(((uint64_t)(index + len) * 100) / total_len);
            otaProgressPct = pct > 100 ? 100 : pct;
          }
        }

        if (final) {
          if (!Update.hasError()) otaProgressPct = 100;
          else                    otaActive = false;
          if (Update.end(true)) {
            Serial.printf("[OTA] Update Success: %uB\n", index + len);
          } else {
            Update.printError(Serial);
          }
        }
      });

  // 6. Wallpaper delete
  server.on("/api/wallpaper-delete", HTTP_POST,
            [](AsyncWebServerRequest *request) {
              if (!web_require_auth(request))
                return;
              if (LittleFS.exists("/wallpaper.jpg"))
                LittleFS.remove("/wallpaper.jpg");
              request->send(200, "text/plain", "RESTART");
              delay(800);
              ESP.restart();
            });

  // 7. Screensaver timeout
  server.on(
      "/api/ss-timeout", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        if (!web_require_auth(request))
          return;
      },
      NULL,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len,
         size_t index, size_t total) {
        if (!web_authed(request))
          return;
        JsonDocument doc(&psramAllocator);
        if (deserializeJson(doc, data, len)) {
          request->send(400, "text/plain", "Bad JSON");
          return;
        }
        int secs = doc["ss_timeout"] | 120;
        if (secs < 30)
          secs = 30;
        if (secs > 600)
          secs = 600;
        screensaverTimeoutMs = (unsigned long)secs * 1000UL;
        preferences.begin(NVS_NAMESPACE, false);
        preferences.putULong("ss_timeout", screensaverTimeoutMs);
        preferences.end();
        request->send(200, "text/plain", "OK");
      });

  // 7b. Apply Wallpaper Preset
  server.on(
      "/api/wallpaper-preset", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        if (!web_require_auth(request))
          return;
      },
      NULL,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len,
         size_t index, size_t total) {
        if (!web_authed(request))
          return;
        JsonDocument doc(&psramAllocator);
        if (deserializeJson(doc, data, len)) {
          request->send(400, "text/plain", "Bad JSON");
          return;
        }
        int id = doc["id"] | 1;
        char presetName[20];
        snprintf(presetName, sizeof(presetName), "/default%d.jpg", id);

        if (LittleFS.exists(presetName)) {
          if (LittleFS.exists("/wallpaper.jpg"))
            LittleFS.remove("/wallpaper.jpg");

          // We use a simple copy by reading and writing
          File src = LittleFS.open(presetName, "r");
          File dst = LittleFS.open("/wallpaper.jpg", "w");
          if (src && dst) {
            uint8_t buf[512];
            while (src.available()) {
              size_t n = src.read(buf, sizeof(buf));
              dst.write(buf, n);
            }
            dst.close();
            src.close();
            Serial.printf("[FS] Preset %d applied to /wallpaper.jpg\n", id);
            request->send(200, "text/plain", "OK");
            delay(500);
            ESP.restart();
          } else {
            if (src) src.close();
            if (dst) dst.close();
            request->send(500, "text/plain", "File Error");
          }
        } else {
          request->send(404, "text/plain", "Preset Not Found");
        }
      });

  // 8. Commercial Diagnostic Logging (SysInfo)
  server.on("/api/sysinfo", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!web_require_auth(request))
      return;
    JsonDocument doc(&psramAllocator);
    doc["uptime_sec"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["free_psram"] = ESP.getFreePsram();
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["ip"] = WiFi.localIP().toString();
    doc["mac"] = WiFi.macAddress();

    // Calculate littleFS usage
    doc["fs_total"] = LittleFS.totalBytes();
    doc["fs_used"] = LittleFS.usedBytes();

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
  });

  // 8b. Rooms API — GET
  // Rooms are discovered from devices, so this only ever reports what already
  // exists; the POST below edits the two fields a room owns on top of that.
  server.on("/api/rooms", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!web_require_auth(request))
      return;

    JsonDocument doc(&psramAllocator);
    JsonArray arr = doc["rooms"].to<JsonArray>();
    for (int i = 0; i < roomCount; i++) {
      JsonObject r = arr.add<JsonObject>();
      r["name"] = rooms[i].name;
      r["icon_type"] = rooms[i].icon_type;
      r["climate_topic"] = rooms[i].climate_topic;
      int on = 0, total = 0;
      room_count_devices(i, &on, &total);
      r["devices"] = total;
      r["climate_valid"] = rooms[i].climateValid;
      if (rooms[i].climateValid) {
        r["temp"] = rooms[i].temp;
        r["hum"] = rooms[i].hum;
      }
    }

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
  });

  // 8c. Rooms API — POST (save icon + climate topic)
  server.on(
      "/api/rooms", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        if (!web_require_auth(request))
          return;
      },
      NULL,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len,
         size_t index, size_t total) {
        if (!web_authed(request))
          return;

        web_collect_json_body(
            request, data, len, index, total, 8192, "/api/rooms",
            [request](JsonDocument &doc) {
          if (!doc["rooms"].is<JsonArray>()) {
            request->send(400, "text/plain", "Missing rooms array");
            return;
          }
          // Matched by name, never by position: the room list is rebuilt from
          // devices on every boot, so an index from the browser could point at
          // a different room by the time it arrives.
          int updated = 0;
          for (JsonObject r : doc["rooms"].as<JsonArray>()) {
            const char *name = r["name"] | "";
            int idx = room_find(name);
            if (idx < 0) continue;
            if (r["icon_type"].is<int>()) {
              int t = r["icon_type"].as<int>();
              rooms[idx].icon_type = (t >= -1 && t <= 9) ? t : ROOM_ICON_AUTO;
            }
            if (r["climate_topic"].is<const char *>()) {
              String ct = r["climate_topic"].as<const char *>();
              ct.trim();
              strncpy(rooms[idx].climate_topic, ct.c_str(),
                      sizeof(rooms[idx].climate_topic) - 1);
              rooms[idx].climate_topic[sizeof(rooms[idx].climate_topic) - 1] = '\0';
              if (!rooms[idx].climate_topic[0]) rooms[idx].climateValid = false;
            }
            updated++;
          }
          if (!saveRooms()) {
            request->send(507, "text/plain", "Storage Error");
            return;
          }
          Serial.printf("[WEB] %d rooms saved — restart to re-subscribe\n",
                        updated);
          request->send(200, "text/plain", "OK");
        });
      });

  // 9. Scenes API — GET
  server.on("/api/scenes", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!web_require_auth(request))
      return;

    JsonDocument doc(&psramAllocator);
    scenesToJson(doc["scenes"].to<JsonArray>());

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
  });

  // 10. Scenes API — POST (save)
  server.on(
      "/api/scenes", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        if (!web_require_auth(request))
          return;
      },
      NULL,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len,
         size_t index, size_t total) {
        if (!web_authed(request))
          return;

        web_collect_json_body(
            request, data, len, index, total, 16384, "/api/scenes",
            [request](JsonDocument &doc) {
          if (!doc["scenes"].is<JsonArray>()) {
            request->send(400, "text/plain", "Missing scenes array");
            return;
          }
          int n = scenesFromJson(doc["scenes"]);
          saveScenes();
          Serial.printf("[WEB] %d scenes saved\n", n);
          request->send(200, "text/plain", "OK");
        });
      });

  // 11. Schedules API — GET
  server.on("/api/schedules", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!web_require_auth(request))
      return;

    JsonDocument doc(&psramAllocator);
    schedulesToJson(doc["schedules"].to<JsonArray>());

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
  });

  // 12. Schedules API — POST (save)
  server.on(
      "/api/schedules", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        if (!web_require_auth(request))
          return;
      },
      NULL,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len,
         size_t index, size_t total) {
        if (!web_authed(request))
          return;

        web_collect_json_body(
            request, data, len, index, total, 4096, "/api/schedules",
            [request](JsonDocument &doc) {
          if (!doc["schedules"].is<JsonArray>()) {
            request->send(400, "text/plain", "Missing schedules array");
            return;
          }
          int n = schedulesFromJson(doc["schedules"]);
          saveSchedules();
          Serial.printf("[WEB] %d schedules saved\n", n);
          request->send(200, "text/plain", "OK");
        });
      });

  // Language API — serve translations as JSON for web portal i18n
  server.on("/api/lang", HTTP_GET, [](AsyncWebServerRequest *request) {
    webActivityDetected = true;
    if (!web_require_auth(request))
      return;
    // Try to serve the JSON file directly from LittleFS
    const char *lang = lang_current();
    if (strcmp(lang, "en") != 0) {
      char path[24];
      snprintf(path, sizeof(path), "/lang_%s.json", lang);
      if (LittleFS.exists(path)) {
        request->send(LittleFS, path, "application/json");
        return;
      }
    }
    // English default — send empty object (JS uses built-in defaults)
    request->send(200, "application/json", "{\"_lang\":\"en\"}");
  });

  // Set language preference - POST /api/lang/set with {code: "th"} or {code: "en"}
  server.on("/api/lang/set", HTTP_POST, [](AsyncWebServerRequest *request) {
    webActivityDetected = true;
    if (!web_require_auth(request))
      return;
    
    if (request->hasParam("plain", true)) {
      String body = request->arg("plain");
      StaticJsonDocument<64> doc;
      if (deserializeJson(doc, body) == DeserializationError::Ok && doc.containsKey("code")) {
        const char *code = doc["code"];
        if ((strcmp(code, "th") == 0 || strcmp(code, "en") == 0)) {
          lang_load(code);
          // Update global language state
          strncpy(currentLang, code, sizeof(currentLang) - 1);
          currentLang[sizeof(currentLang) - 1] = '\0';
          // Persist preference - use same namespace as loadSettings()
          Preferences prefs;
          prefs.begin(NVS_NAMESPACE, false);
          prefs.putString("lang", String(code));
          prefs.end();
          request->send(200, "application/json", "{\"ok\":true}");
          return;
        }
      }
    }
    request->send(400, "application/json", "{\"error\":\"Invalid language code\"}");
  });

  // Language file upload — POST /api/lang/upload?code=th
  // Allows uploading lang_xx.json without reflashing LittleFS
  server.on(
      "/api/lang/upload", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        if (!web_require_auth(request))
          return;
        request->send(200, "text/plain", "OK");
      },
      [](AsyncWebServerRequest *request, const String &filename, size_t index,
         uint8_t *data, size_t len, bool final) {
        static File langFile;
        if (index == 0) {
          String code = request->hasParam("code") ? request->getParam("code")->value() : "th";
          // Sanitize: only allow 2-3 lowercase letters
          if (code.length() < 2 || code.length() > 3) code = "th";
          for (unsigned i = 0; i < code.length(); i++) {
            if (code[i] < 'a' || code[i] > 'z') { code = "th"; break; }
          }
          char path[24];
          snprintf(path, sizeof(path), "/lang_%s.json", code.c_str());
          langFile = LittleFS.open(path, "w");
          Serial.printf("[LANG] Uploading %s (%u bytes)\n", path, request->contentLength());
        }
        if (langFile && len > 0) langFile.write(data, len);
        if (final && langFile) {
          langFile.close();
          Serial.printf("[LANG] Upload complete (%u bytes)\n", index + len);
        }
      });

  // Stock Ticker Config — GET
  server.on("/api/stock-config", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!web_require_auth(request))
      return;
    JsonDocument doc(&psramAllocator);
    doc["enabled"] = stockEnabled;
    doc["symbol0"] = stockSymbols[0];
    doc["symbol1"] = stockSymbols[1];
    doc["symbol2"] = stockSymbols[2];
    doc["api_key"] = (strlen(stockApiKey) > 0) ? "********" : "";
    AsyncResponseStream *resp = request->beginResponseStream("application/json");
    serializeJson(doc, *resp);
    request->send(resp);
  });

  // Stock Ticker Config — POST (save to NVS)
  server.on(
      "/api/stock-config", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        if (!web_require_auth(request))
          return;
      },
      NULL,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len,
         size_t index, size_t total) {
        if (!web_authed(request))
          return;
        web_collect_json_body(
            request, data, len, index, total, 512, "/api/stock-config",
            [request](JsonDocument &doc) {
          stockEnabled = doc["enabled"] | false;
          // Empty fields leave the stored value alone.
          auto copyStr = [](const char *src, char *dst, size_t dsz) {
            if (src && strlen(src) > 0) {
              strncpy(dst, src, dsz - 1);
              dst[dsz - 1] = '\0';
            }
          };
          copyStr(doc["symbol0"] | "", stockSymbols[0], sizeof(stockSymbols[0]));
          copyStr(doc["symbol1"] | "", stockSymbols[1], sizeof(stockSymbols[1]));
          copyStr(doc["symbol2"] | "", stockSymbols[2], sizeof(stockSymbols[2]));
          const char *newKey = doc["api_key"] | "";
          if (newKey && strlen(newKey) > 0 && strcmp(newKey, "********") != 0) {
            strncpy(stockApiKey, newKey, sizeof(stockApiKey) - 1);
            stockApiKey[sizeof(stockApiKey) - 1] = '\0';
          }
          saveStockConfig();
          // Force screensaver rebuild so stock bar appears/disappears correctly
          invalidate_screensaver_build();
          // Force immediate fetch on next network_loop() tick
          lastStockUpdate = millis() - STOCK_UPDATE_MS - 1;
          Serial.printf("[WEB] Stock config saved: enabled=%d s0=%s\n",
                        stockEnabled, stockSymbols[0]);
          request->send(200, "text/plain", "OK");
        });
      });

  // Catch any unmatched routes for diagnostics
  server.onNotFound([](AsyncWebServerRequest *request) {
    webActivityDetected = true;
    Serial.printf("[WEB] 404 %s %s from %s\n",
                  request->methodToString(),
                  request->url().c_str(),
                  request->client()->remoteIP().toString().c_str());
    request->send(404, "text/plain", "Not Found");
  });

  // Start the server
  server.begin();
  Serial.println("Async Web Server started on port 80");
}
