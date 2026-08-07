#include "mqtt_manager.h"
#include "ui.h"
#include "ui/ui_dimmer_modal.h"
#include "ui/ui_screens.h" // ui_refresh_home_climate()
#include "globals.h"
#include <ArduinoJson.h>
#include <MQTT.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern WiFiClient espClient;
MQTTClient mqttClient(MQTT_BUFFER_SIZE);

// ── Offline Command Queue ──────────────────────────────
// Ring buffer of MQTT commands queued while broker is offline.
// Replayed automatically on reconnect.

struct MqttQueueItem {
  char topic[96];
  char payload[48];
  bool used;
  bool retain;
};

static MqttQueueItem mqttQueue[MQTT_QUEUE_SIZE];
static int mqttQueueHead = 0; // next write position
static SemaphoreHandle_t mqtt_queue_mux = NULL;
static volatile bool mqttStateSyncRequested = false;

static void mqtt_enqueue(const char *topic, const char *payload, bool retain = false) {
  if (!mqtt_queue_mux) mqtt_queue_mux = xSemaphoreCreateMutex();
  if (xSemaphoreTake(mqtt_queue_mux, pdMS_TO_TICKS(50)) != pdTRUE) return;
  strncpy(mqttQueue[mqttQueueHead].topic, topic, sizeof(mqttQueue[0].topic) - 1);
  mqttQueue[mqttQueueHead].topic[sizeof(mqttQueue[0].topic) - 1] = '\0';
  strncpy(mqttQueue[mqttQueueHead].payload, payload, sizeof(mqttQueue[0].payload) - 1);
  mqttQueue[mqttQueueHead].payload[sizeof(mqttQueue[0].payload) - 1] = '\0';
  mqttQueue[mqttQueueHead].used = true;
  mqttQueue[mqttQueueHead].retain = retain;
  mqttQueueHead = (mqttQueueHead + 1) % MQTT_QUEUE_SIZE;
  xSemaphoreGive(mqtt_queue_mux);
  Serial.printf("[MQTT-Q] Queued: %s -> %s (retain=%d)\n", topic, payload, retain);
}

void mqtt_flush_queue() {
  if (!mqtt_queue_mux) mqtt_queue_mux = xSemaphoreCreateMutex();
  if (xSemaphoreTake(mqtt_queue_mux, pdMS_TO_TICKS(50)) != pdTRUE) return;
  int flushed = 0;
  for (int i = 0; i < MQTT_QUEUE_SIZE; i++) {
    int idx = (mqttQueueHead + i) % MQTT_QUEUE_SIZE; // oldest first
    if (mqttQueue[idx].used) {
      mqttClient.publish(mqttQueue[idx].topic, mqttQueue[idx].payload, mqttQueue[idx].retain, 1);
      Serial.printf("[MQTT-Q] Sent: %s -> %s (retain=%d)\n", mqttQueue[idx].topic, mqttQueue[idx].payload, mqttQueue[idx].retain);
      mqttQueue[idx].used = false;
      flushed++;
    }
  }
  if (flushed > 0) {
    Serial.printf("[MQTT-Q] Flushed %d commands\n", flushed);
  }
  xSemaphoreGive(mqtt_queue_mux);
}

void mqtt_callback(MQTTClient *client, char topic[], char bytes[], int length) {
  String msg(bytes, length);
  msg.trim();
  Serial.printf("[MQTT IN] Topic: '%s' Len: %u Payload: '%s'\n", topic, length,
                msg.c_str());

  if (xSemaphoreTake(devices_mux, pdMS_TO_TICKS(200)) != pdTRUE) {
    Serial.println("[MQTT] Could not acquire devices_mux, dropping message");
    return;
  }

  bool matched = false;
  for (int i = 0; i < deviceCount; i++) {
    // Avoid String object creation in a loop over 100 devices
    bool state_match = (devices[i].state_topic[0] != '\0' && strcmp(topic, devices[i].state_topic) == 0);
    bool cmnd_match = (devices[i].cmnd_topic[0] != '\0' && strcmp(topic, devices[i].cmnd_topic) == 0);
    bool dimmer_match = (devices[i].dimmer_topic[0] != '\0' && strcmp(topic, devices[i].dimmer_topic) == 0);
    
    if (state_match || cmnd_match || dimmer_match) {
      matched = true;
      devices[i].lastSeenTime = millis(); // track last message time
      devices[i].pendingSince = 0;        // broker answered — no longer pending
      
      Serial.printf("[MQTT-MATCH] Device %d '%s': ", i, devices[i].name);
      if (state_match) Serial.print("STATE_TOPIC ");
      if (cmnd_match) Serial.print("CMND_TOPIC ");
      if (dimmer_match) Serial.print("DIMMER_TOPIC ");
      Serial.println();

      bool state = false;
      bool found_power = false;
      bool has_explicit_state = false; // True only when payload actually carries ON/OFF info

      // 1. JSON match {"POWER":"ON"}, {"state":"on"}
      if (msg.startsWith("{") && msg.endsWith("}")) {
        Serial.println("[MQTT-PARSE] Attempting JSON parse...");
        JsonDocument jdoc;
        DeserializationError jerr = deserializeJson(jdoc, msg);
        if (!jerr) {
          Serial.println("[MQTT-JSON-OK] JSON deserialized successfully");
          // Check POWER, state, On, on, Status fields (covers Tasmota + Homebridge)
          const char *power_val = jdoc["POWER"] | (const char *)nullptr;
          if (!power_val) power_val = jdoc["state"] | (const char *)nullptr;
          if (!power_val) power_val = jdoc["State"] | (const char *)nullptr;
          if (!power_val) power_val = jdoc["Status"] | (const char *)nullptr;
          if (power_val) {
            state = (strcasecmp(power_val, "ON") == 0 || strcasecmp(power_val, "true") == 0 ||
                     strcasecmp(power_val, "1") == 0);
            found_power = true;
            has_explicit_state = true;
            Serial.printf("[MQTT-JSON-STR] Found string key, value='%s' -> state=%d\n", power_val, state);
          }
          // Also handle boolean JSON: {"On":true}, {"on":true}, {"state":true}
          if (!found_power) {
            Serial.println("[MQTT-JSON-BOOL] Searching for boolean keys...");
            for (const char *key : {"On", "on", "POWER", "state", "State", "Status"}) {
              if (jdoc[key].is<bool>()) {
                state = jdoc[key].as<bool>();
                found_power = true;
                has_explicit_state = true;
                Serial.printf("[MQTT-JSON-BOOL] Found bool key '%s' -> state=%d\n", key, state);
                break;
              }
            }
          }

          // Check for brightness (Dimmer / dimmer / Brightness / brightness)
          int b = -1;
          if (jdoc["Dimmer"].is<int>()) b = jdoc["Dimmer"].as<int>();
          else if (jdoc["dimmer"].is<int>()) b = jdoc["dimmer"].as<int>();
          else if (jdoc["Brightness"].is<int>()) b = jdoc["Brightness"].as<int>();
          else if (jdoc["brightness"].is<int>()) b = jdoc["brightness"].as<int>();
          if (b >= 0 && b <= 100) {
            devices[i].brightness = b;
            if (xSemaphoreTake(lvgl_mux, pdMS_TO_TICKS(100)) == pdTRUE) {
              update_dimmer_modal_value(i, b);
              // Fan and AC tiles render the level itself, so an inbound value
              // has to repaint the tile — not just the brightness modal.
              ui_update_device_status(i, devices[i].status);
              xSemaphoreGive(lvgl_mux);
            }
            Serial.printf("[MQTT-BRIGHTNESS] Updated: %d%%\n", b);
          }
        } else {
          Serial.printf("[MQTT-JSON-FAIL] Deserialization error: %s\n", jerr.c_str());
        }
      } else {
        Serial.println("[MQTT-PARSE] Not JSON, trying direct string match...");
      }

      // 2. Direct matching for dimmer topic with numeric payload
      if (strcmp(topic, devices[i].dimmer_topic) == 0 && msg.length() > 0 &&
          isDigit(msg[0])) {
        int b = msg.toInt();
        if (b >= 0 && b <= 100) {
          devices[i].brightness = b;
          if (xSemaphoreTake(lvgl_mux, pdMS_TO_TICKS(100)) == pdTRUE) {
            update_dimmer_modal_value(i, b);
            xSemaphoreGive(lvgl_mux);
          }
          Serial.printf("[MQTT-DIMMER-NUM] Updated: %d%%\n", b);
        }
      } else if (!found_power) {
        // Direct string match for simple ON/OFF payloads. Only treat the
        // update as an explicit state change if the payload is one of the
        // recognised on/off tokens — otherwise an unrelated payload (e.g. a
        // bare dimmer JSON without a power field) would silently flip state
        // to OFF.
        String token = msg;
        token.toLowerCase();
        if (token == "on" || token == "1" || token == "true") {
          state = true;
          has_explicit_state = true;
        } else if (token == "off" || token == "0" || token == "false") {
          state = false;
          has_explicit_state = true;
        }
        Serial.printf("[MQTT-STRING] Parsed direct payload -> state=%d explicit=%d\n",
                      state, has_explicit_state);
      }

      // --- DEBOUNCE / FLAP-SUPPRESSION ---
      // After a local tap (markToggled), ignore inbound state messages for
      // DEVICE_DEBOUNCE_MS so optimistic UI doesn't get bounced by the broker's
      // echo. If a contrary state arrives during this window we ALSO re-assert
      // our desired state on the retained stat topic, which overwrites any
      // stale retained value left by other publishers (Homebridge, Tasmota,
      // etc.) and breaks ON/OFF flapping loops.
      if (devices[i].isDebouncing()) {
        if (has_explicit_state && state != devices[i].status) {
          Serial.printf("[MQTT-FLAP] Contrary state during debounce for '%s' "
                        "(want=%d, got=%d). Re-asserting on stat topic.\n",
                        devices[i].name, devices[i].status, state);
          if (devices[i].state_topic[0] != '\0') {
            const char *desiredPayload = devices[i].status ? "ON" : "OFF";
            // Safe inside callback: QoS 0, retained, single small publish.
            mqttClient.publish(devices[i].state_topic, desiredPayload, true, 0);
          }
        } else {
          Serial.printf("[MQTT-DEBOUNCE] Ignoring state update for Device '%s' (idx=%d). "
                        "Waiting for settle.\n",
                        devices[i].name, i);
        }
        break;
      }

      Serial.printf("[MQTT-STATE-CHECK] Device '%s': Current=%d, New=%d explicit=%d\n",
                    devices[i].name, devices[i].status, state, has_explicit_state);

      if (has_explicit_state && devices[i].status != state) {
        Serial.printf("[MQTT-UPDATE] Device '%s' state changed from %d to %d\n", 
                      devices[i].name, devices[i].status, state);
        devices[i].updateState(state);
        deviceStatesDirty = true;
        
        Serial.printf("[MQTT-UI-UPDATE] Calling ui_update_device_status for device '%s' (idx=%d), state=%d\n",
                      devices[i].name, i, state);
        if (xSemaphoreTake(lvgl_mux, pdMS_TO_TICKS(100)) == pdTRUE) {
          ui_update_device_status(i, state);
          Serial.printf("[MQTT-UI-DONE] UI update complete for device '%s'\n", devices[i].name);
          xSemaphoreGive(lvgl_mux);
        } else {
          Serial.printf("[MQTT-UI-FAIL] Could not acquire lvgl_mux for device '%s'\n", devices[i].name);
        }

        // If command came on /set (from HomeKit), echo state to /stat with retain
        // so broker remembers and Homebridge getOn stays in sync
        if (strcmp(topic, devices[i].cmnd_topic) == 0 &&
            devices[i].state_topic[0] != '\0') {
          const char *retainPayload = state ? "ON" : "OFF";
          mqttClient.publish(devices[i].state_topic, retainPayload, true, 0); // retain=true, QoS 0 (safe inside callback)
          Serial.printf("[MQTT-ECHO] Echo to stat (retained): %s -> %s\n",
                        devices[i].state_topic, retainPayload);
        }
      } else {
        Serial.printf("[MQTT-NO-CHANGE] State unchanged for device '%s': %s\n", 
                      devices[i].name, state ? "ON" : "OFF");
      }
      break;
    }
  }
  // Room climate. Checked only when no device claimed the topic, so a room can
  // never shadow a device that happens to share one.
  if (!matched) {
    for (int r = 0; r < roomCount; r++) {
      if (!rooms[r].climate_topic[0]) continue;
      if (strcmp(topic, rooms[r].climate_topic) != 0) continue;
      matched = true;

      // Accept the short keys the design uses and the long ones most sensor
      // firmwares publish. A payload carrying only one of the two updates only
      // that reading rather than zeroing the other.
      float t = NAN;
      int h = -1;
      if (msg.startsWith("{") && msg.endsWith("}")) {
        JsonDocument jd;
        if (!deserializeJson(jd, msg)) {
          if (jd["t"].is<float>())                 t = jd["t"].as<float>();
          else if (jd["temp"].is<float>())         t = jd["temp"].as<float>();
          else if (jd["temperature"].is<float>())  t = jd["temperature"].as<float>();
          if (jd["h"].is<int>())                   h = jd["h"].as<int>();
          else if (jd["hum"].is<int>())            h = jd["hum"].as<int>();
          else if (jd["humidity"].is<int>())       h = jd["humidity"].as<int>();
        }
      } else {
        // A bare number is a temperature — the common case for a lone sensor.
        t = msg.toFloat();
      }

      bool changed = false;
      if (!isnan(t) && t > -50.0f && t < 100.0f) { rooms[r].temp = t; changed = true; }
      if (h >= 0 && h <= 100)                    { rooms[r].hum = h;  changed = true; }
      if (!changed) {
        Serial.printf("[ROOM-CLIMATE] '%s': unusable payload\n", rooms[r].name);
        break;
      }
      rooms[r].climateValid = true;
      Serial.printf("[ROOM-CLIMATE] '%s': %.1fC %d%%\n", rooms[r].name,
                    rooms[r].temp, rooms[r].hum);

      // Home renders the climate line from these fields, so the cards have to
      // be rebuilt — unlike a device state change, there is no per-widget
      // update path for it.
      if (xSemaphoreTake(lvgl_mux, pdMS_TO_TICKS(100)) == pdTRUE) {
        ui_refresh_home_climate();
        xSemaphoreGive(lvgl_mux);
      }
      break;
    }
  }

  if (!matched) {
    Serial.println("[MQTT-NO-MATCH] No device matched incoming MQTT message");
    Serial.printf("[MQTT-DEVICES-DEBUG] Total devices: %d\n", deviceCount);
    for (int i = 0; i < deviceCount; i++) {
      if (strlen(devices[i].state_topic) > 0 ||
          strlen(devices[i].cmnd_topic) > 0) {
        Serial.printf("  Dev %d '%s': state='%s' cmnd='%s' dimmer='%s'\n", 
                      i, devices[i].name, devices[i].state_topic,
                      devices[i].cmnd_topic, devices[i].dimmer_topic);
      }
    }
  }
  xSemaphoreGive(devices_mux);
}

void reconnect_mqtt() {
  if (!mqttClient.connected()) {
    String clientId = "ESP32Panel-" + WiFi.macAddress();
    // Set LWT and persistent session before connect
    mqttClient.setWill(PANEL_STATUS_TOPIC, "offline", true, 1);
    mqttClient.setCleanSession(false); // broker queues QoS 1 messages while we're offline
    if (mqttClient.connect(clientId.c_str(), mqtt_username.c_str(),
                           mqtt_password.c_str())) {
      Serial.println("[MQTT] Connected to broker (persistent session)!");
      isMqttConnected = true;
      mqttClient.publish(PANEL_STATUS_TOPIC, "online", true, 1);
      
      if (xSemaphoreTake(lvgl_mux, pdMS_TO_TICKS(100)) == pdTRUE) {
        ui_update_header();
        xSemaphoreGive(lvgl_mux);
      }
      
      for (int i = 0; i < deviceCount; i++) {
        Serial.printf("[MQTT-SUB] Device %d '%s':\n", i, devices[i].name);
        if (strlen(devices[i].state_topic) > 0) {
          bool ok = mqttClient.subscribe(devices[i].state_topic, 1); // QoS 1
          Serial.printf("  STATE  [%s]: '%s'\n", ok ? "OK" : "FAIL", devices[i].state_topic);
        } else {
          Serial.println("  STATE  [skip]: <empty>");
        }
        if (strlen(devices[i].cmnd_topic) > 0 &&
            strcmp(devices[i].cmnd_topic, devices[i].state_topic) != 0) {
          bool ok = mqttClient.subscribe(devices[i].cmnd_topic, 1); // QoS 1
          Serial.printf("  CMND   [%s]: '%s'\n", ok ? "OK" : "FAIL", devices[i].cmnd_topic);
        } else if (strlen(devices[i].cmnd_topic) == 0) {
          Serial.println("  CMND   [skip]: <empty>");
        } else {
          Serial.println("  CMND   [skip]: same as STATE");
        }
        if (strlen(devices[i].dimmer_topic) > 0) {
          bool ok = mqttClient.subscribe(devices[i].dimmer_topic, 1); // QoS 1
          Serial.printf("  DIMMER [%s]: '%s'\n", ok ? "OK" : "FAIL", devices[i].dimmer_topic);
        }
      }

      // Room climate sensors — retained, so the reading lands right away.
      for (int r = 0; r < roomCount; r++) {
        if (!rooms[r].climate_topic[0]) continue;
        bool ok = mqttClient.subscribe(rooms[r].climate_topic, 1);
        Serial.printf("[MQTT-SUB] Room '%s' climate [%s]: '%s'\n", rooms[r].name,
                      ok ? "OK" : "FAIL", rooms[r].climate_topic);
      }

      network_request_all_states();

#if MQTT_DEBUG_SNIFF_HOMEBRIDGE
      // Debug: dump every message under homebridge/# to find the real topics
      if (mqttClient.subscribe("homebridge/#", 0)) {
        Serial.println("[MQTT-SNIFF] Subscribed to homebridge/# (debug)");
      } else {
        Serial.println("[MQTT-SNIFF] FAILED to subscribe homebridge/#");
      }
#endif

      // Replay any commands that were queued while offline
      mqtt_flush_queue();

      // --- HA MQTT Auto-Discovery ---
      // Publish config for each device so Home Assistant auto-detects them
      for (int i = 0; i < deviceCount; i++) {
        if (strlen(devices[i].cmnd_topic) == 0) continue;
        // Build a unique object_id from device name (replace spaces with _)
        char obj_id[64];
        strncpy(obj_id, devices[i].name, sizeof(obj_id) - 1);
        obj_id[sizeof(obj_id) - 1] = '\0';
        for (char *p = obj_id; *p; p++) { if (*p == ' ') *p = '_'; }
        // Convert to lowercase
        for (char *p = obj_id; *p; p++) { if (*p >= 'A' && *p <= 'Z') *p += 32; }

        char disc_topic[128];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/switch/sc01_%s/config", obj_id);

        JsonDocument doc;
        doc["name"] = devices[i].name;
        char uid[80];
        snprintf(uid, sizeof(uid), "sc01_%s", obj_id);
        doc["unique_id"] = uid;
        doc["command_topic"] = devices[i].cmnd_topic;
        doc["state_topic"] = devices[i].state_topic;
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["availability_topic"] = PANEL_STATUS_TOPIC;
        doc["payload_available"] = "online";
        doc["payload_not_available"] = "offline";
        JsonObject dev = doc["device"].to<JsonObject>();
        dev["identifiers"][0] = "sc01_smart_panel";
        dev["name"] = "SC01 Smart Panel";
        dev["manufacturer"] = "WT32-SC01";
        dev["model"] = "Plus";

        char buf[384];
        size_t len = serializeJson(doc, buf, sizeof(buf));
        mqttClient.publish(disc_topic, buf, len, true, 1);
        Serial.printf("[HA-DISC] %s\n", disc_topic);
      }
    } else {
      Serial.printf("[MQTT] Connect failed, err=%d. Retrying in 5s\n",
                    (int)mqttClient.lastError());
      isMqttConnected = false;
      if (xSemaphoreTake(lvgl_mux, pdMS_TO_TICKS(100)) == pdTRUE) {
        ui_update_header();
        xSemaphoreGive(lvgl_mux);
      }
    }
  }
}

void mqtt_manager_setup() {
  mqttClient.begin(mqtt_server_ip.c_str(), mqtt_port, espClient);
  mqttClient.onMessageAdvanced(mqtt_callback);

  if (isWifiConnected) {
    reconnect_mqtt();
  }
}

void mqtt_manager_loop() {
  if (isWifiConnected) {
    if (!mqttClient.connected()) {
      isMqttConnected = false;
      if (xSemaphoreTake(lvgl_mux, pdMS_TO_TICKS(100)) == pdTRUE) {
        ui_update_header();
        xSemaphoreGive(lvgl_mux);
      }
      static unsigned long lastMqttReconnectAttempt = 0;
      static unsigned long mqttBackoff = MQTT_BACKOFF_INIT_MS;
      if (millis() - lastMqttReconnectAttempt > mqttBackoff) {
        lastMqttReconnectAttempt = millis();
        reconnect_mqtt();
        if (isMqttConnected) {
          mqttBackoff = MQTT_BACKOFF_INIT_MS; // reset on success
        } else {
          mqttBackoff = min(mqttBackoff * 2, (unsigned long)MQTT_BACKOFF_MAX_MS);
          Serial.printf("[MQTT] Next retry in %lus\n", mqttBackoff / 1000);
        }
      }
    } else {
      mqttClient.loop();
      mqtt_flush_queue();  // Flush any commands enqueued from other cores

      if (mqttStateSyncRequested) {
        mqttStateSyncRequested = false;
        network_request_all_states();
      }

      // Periodic state sync — re-subscribe to force broker to re-send retained messages
      unsigned long syncInterval = screensaverActive
          ? STATUS_SYNC_SAVER_MS
          : STATUS_SYNC_ACTIVE_MS;
      if (millis() - lastStatusSync > syncInterval) {
        lastStatusSync = millis();
        network_request_all_states();
      }

      // Debounced save of device states to flash (max every 10 seconds)
      static unsigned long lastStateSave = 0;
      if (deviceStatesDirty && millis() - lastStateSave > DEVICE_STATE_SAVE_MS) {
        lastStateSave = millis();
        deviceStatesDirty = false;
        saveDevices();
        Serial.println("[MQTT] Device states saved to flash");
      }

      // Periodic MQTT heartbeat for debugging (every 30 seconds)
      static unsigned long lastHeartbeat = 0;
      if (millis() - lastHeartbeat > MQTT_HEARTBEAT_MS) {
        lastHeartbeat = millis();
        mqttClient.publish(PANEL_STATUS_TOPIC, "online", true, 1);
        Serial.printf("[MQTT] Heartbeat: connected=%d, devices=%d\n",
                      mqttClient.connected(), deviceCount);
      }
    }
  }
}

void toggle_device(int index) {
  Serial.printf("[CLICK] idx:%d WiFi:%s MQTT:%s\n", index,
                isWifiConnected ? "OK" : "X", isMqttConnected ? "OK" : "X");
  Serial.flush();

  if (xSemaphoreTake(devices_mux, pdMS_TO_TICKS(200)) != pdTRUE) {
    ui_show_toast("Busy...");
    return;
  }

  bool newState = !devices[index].status;

  // Optimistic UI update
  devices[index].updateState(newState);
  devices[index].markToggled();
  deviceStatesDirty = true;
  ui_update_device_status(index, newState);

  const char *cmnd = devices[index].cmnd_topic;
  const char *stat = devices[index].state_topic;
  String payload = newState ? "ON" : "OFF";

  // Queue publishes here and let the network task flush them. Direct MQTT
  // access from the UI task can race mqttClient.loop() on the network task
  // and cause spontaneous resets when the panel is tapped.
  if (cmnd[0] != '\0') {
    mqtt_enqueue(cmnd, payload.c_str(), false); // command -> not retained
    Serial.printf("[MQTT-Q] Local toggle CMND: %s -> %s\n", cmnd,
                  payload.c_str());
  }
  if (stat[0] != '\0') {
    mqtt_enqueue(stat, payload.c_str(), true); // state -> retained so subscribers see latest
    Serial.printf("[MQTT-Q] Local toggle STAT: %s -> %s\n", stat,
                  payload.c_str());
  }

  if (cmnd[0] == '\0' && stat[0] == '\0') {
    devices[index].updateState(!newState);
    devices[index].clearDebounce();
    devices[index].pendingSince = 0;
    ui_update_device_status(index, !newState);
    ui_show_toast("No topic!");
    Serial.println("[MQTT-Q] Local toggle aborted: no topics configured");
  } else {
    // Command is out; the tile shows the optimistic state but marks itself
    // unconfirmed until the broker echoes something back on one of the
    // device's topics (see the pendingSince reset in the message handler).
    devices[index].pendingSince = millis();
    ui_show_toast(isMqttConnected ? "Sent" : "Queued (offline)");
  }

  xSemaphoreGive(devices_mux);
  Serial.flush();
}

// Sends a device's numeric channel — brightness, fan speed or AC setpoint,
// depending on its type. Mirrors toggle_device(): optimistic UI first, then a
// queued publish, because direct MQTT access from the UI task races
// mqttClient.loop() on the network task.
void set_device_level(int index, int level) {
  if (index < 0 || index >= deviceCount || index >= MAX_DEVICES) return;
  Device &d = devices[index];
  if (!d.hasLevel()) return;

  level = d.clampLevel(level);
  if (xSemaphoreTake(devices_mux, pdMS_TO_TICKS(200)) != pdTRUE) {
    ui_show_toast("Busy...");
    return;
  }

  d.brightness = level;
  deviceStatesDirty = true;

  // A fan dropped to 0 is off; raising a fan or dimmer off zero turns it on.
  // The power command goes out separately from the level so devices that only
  // understand ON/OFF still follow.
  bool power_changed = false;
  if (d.levelImpliesOff(level) && d.status) {
    d.updateState(false);
    power_changed = true;
  } else if (d.levelImpliesOn(level) && !d.status) {
    d.updateState(true);
    power_changed = true;
  }
  if (power_changed) {
    d.markToggled();
    const char *payload = d.status ? "ON" : "OFF";
    if (d.cmnd_topic[0]) mqtt_enqueue(d.cmnd_topic, payload, false);
    if (d.state_topic[0]) mqtt_enqueue(d.state_topic, payload, true);
  }

  // Same fallback the brightness modal has always used: derive a level topic
  // from the command topic when none is configured.
  char payload[8];
  snprintf(payload, sizeof(payload), "%d", level);
  if (d.dimmer_topic[0]) {
    mqtt_enqueue(d.dimmer_topic, payload, false);
    Serial.printf("[MQTT-Q] Level %s -> %s\n", d.dimmer_topic, payload);
  } else if (d.cmnd_topic[0]) {
    String t = d.cmnd_topic;
    t.replace("/POWER", "/Dimmer");
    t.replace("/power", "/dimmer");
    mqtt_enqueue(t.c_str(), payload, false);
    Serial.printf("[MQTT-Q] Level %s -> %s (derived)\n", t.c_str(), payload);
  } else {
    ui_show_toast("No topic!");
  }

  ui_update_device_status(index, d.status);
  xSemaphoreGive(devices_mux);
}

void network_request_all_states() {
  if (!isMqttConnected)
    return;

  // Only re-subscribe to STATE topics (which carry retained messages from broker).
  // For command topics, re-assert the subscription without unsubscribing first.
  // This keeps the command path alive across broker/session glitches while still
  // avoiding a gap where Homebridge/Apple Home commands can be missed.
  Serial.println("[MQTT] Syncing — re-subscribing state topics and refreshing command subscriptions...");
  for (int i = 0; i < deviceCount; i++) {
    if (strlen(devices[i].state_topic) > 0) {
      // Unsubscribe then re-subscribe to force broker to re-deliver retained messages
      mqttClient.unsubscribe(devices[i].state_topic);
      mqttClient.subscribe(devices[i].state_topic, 1); // QoS 1
      Serial.printf("  Re-sub STATE: %s\n", devices[i].state_topic);
      yield();
    }

    if (strlen(devices[i].cmnd_topic) > 0 &&
        strcmp(devices[i].cmnd_topic, devices[i].state_topic) != 0) {
      bool ok = mqttClient.subscribe(devices[i].cmnd_topic, 1);
      Serial.printf("  Keep CMND [%s]: %s\n", ok ? "OK" : "FAIL",
                    devices[i].cmnd_topic);
      yield();
    }

    if (strlen(devices[i].dimmer_topic) > 0) {
      bool ok = mqttClient.subscribe(devices[i].dimmer_topic, 1);
      Serial.printf("  Keep DIMM [%s]: %s\n", ok ? "OK" : "FAIL",
                    devices[i].dimmer_topic);
      yield();
    }
  }
}

void request_network_state_sync() {
  mqttStateSyncRequested = true;
}

void mqtt_publish_string(const char *topic, const char *payload) {
  // Always use queue — safe to call from any core/task.
  // Queue is flushed from the network task on Core 0.
  mqtt_enqueue(topic, payload);
}
