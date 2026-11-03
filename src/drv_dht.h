#pragma once
// Sterownik DHT11 (1-Wire, GPIO PIN_DHT) - temperatura i wilgotnosc.
// Odczyt co ~2 s (DHT11 wymaga >=1 s miedzy probkami).
#include <Arduino.h>

struct DhtData {
    float temp;     // [deg C]
    float hum;      // [% RH]
    bool  valid;
    uint32_t lastMs;
};

void  dht_begin();
void  dht_loop();               // wywolywac w loop()
DhtData dht_get();
