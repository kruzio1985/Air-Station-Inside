#pragma once
// MQTT + Home Assistant discovery + subskrypcja Zigbee2MQTT (pojemniki).
#include <Arduino.h>

void mqtt_begin();
void mqtt_loop();
bool mqtt_connected();
void mqtt_apply_settings();   // po zmianie ustawien brokera (restart klienta)
