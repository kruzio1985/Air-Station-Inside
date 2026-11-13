#pragma once
// Zewnetrzne czujniki z Home Assistant (REST API):
//  - temperatura/wilgotnosc (odczyt co HA_POLL_MS),
//  - czujnik obecnosci (binary_sensor, odczyt co HA_PRESENCE_POLL_MS).
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

// Obecnosc z HA. Zwraca true gdy czujnik "on" (lub gdy obsluga wylaczona).
// Przy braku odczytu (unavailable) zwraca true, zeby nie gasic LED blednie.
bool  ha_presence();
