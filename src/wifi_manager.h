#pragma once
// wifi_manager.h
// Declares WiFi/MQTT/OTA functions and includes globals.h so all callers get
// access to shared state via a single include.

#include "config.h"
#include "globals.h"
#include "mqtt_manager.h" // toggle_device, reconnect_mqtt, network_request_all_states

void network_load_persistence();
void network_setup();
void network_loop();

// Generic MQTT publish helper (used by UI components like Dimmer modal)
void mqtt_publish_string(const char *topic, const char *payload);
