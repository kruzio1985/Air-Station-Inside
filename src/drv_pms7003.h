#pragma once
// Sterownik PMS7003 (UART2, 9600, tryb aktywny - ramki 32 B co ~1 s).
#include <Arduino.h>

struct PmsData {
    float pm1;     // PM1.0  CF=1 [ug/m3]
    float pm25;    // PM2.5  CF=1 [ug/m3]
    float pm10;    // PM10   CF=1 [ug/m3]
    bool  valid;
    uint32_t lastMs;
};

void  pms_begin();
void  pms_loop();               // wywolywac w loop() - parsuje bufor UART
PmsData pms_get();
