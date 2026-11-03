#pragma once
// Sterownik 2x SHT31 (bez pinu ADDR -> staly adres 0x44).
//   #1: wewnetrzny  - I2C #2 (Wire1): SDA=GPIO47, SCL=GPIO40
//   #2: zewnetrzny  - I2C glowna (Wire): SDA=GPIO8, SCL=GPIO9 (obok ADS1115 0x48)
#include <Arduino.h>

struct Sht31Data {
    float temp;     // [deg C]
    float hum;      // [% RH]
    bool  valid;
    uint32_t lastMs;
};

void  sht31_begin();
void  sht31_loop();               // wywolywac w loop()
Sht31Data sht31_get(int idx);     // 0 = 0x44 (wewn.), 1 = 0x45 (zewn.)
