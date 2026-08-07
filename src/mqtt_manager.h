#pragma once
#include "config.h"
#include "globals.h"

void mqtt_manager_setup();
void mqtt_manager_loop();
void mqtt_publish_string(const char *topic, const char *payload);
void toggle_device(int index);
// Publishes the device's numeric channel (brightness / fan speed / AC
// setpoint) and, where the type implies it, the matching power command.
void set_device_level(int index, int level);
void network_request_all_states();
void request_network_state_sync();
void reconnect_mqtt();
void mqtt_flush_queue(); // replay queued commands after reconnect
