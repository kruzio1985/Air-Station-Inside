#pragma once
// Zewnetrzny czujnik temperatury/wilgotnosci z Home Assistant (REST API).
// Odczyt co HA_POLL_MS (domyslnie 60 s) - entity i token w konfiguracji.
#include <Arduino.h>

struct HaData {
    float temp;     // [deg C]
    float hum;      // [% RH]
    bool  valid;
    uint32_t lastMs;
};

void  ha_begin();
void  ha_loop();
HaData ha_get();
