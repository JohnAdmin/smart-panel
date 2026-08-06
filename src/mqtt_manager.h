#pragma once
#include "config.h"
#include "globals.h"

void mqtt_manager_setup();
void mqtt_manager_loop();
void mqtt_publish_string(const char *topic, const char *payload);
void toggle_device(int index);
void network_request_all_states();
void request_network_state_sync();
void reconnect_mqtt();
void mqtt_flush_queue(); // replay queued commands after reconnect
