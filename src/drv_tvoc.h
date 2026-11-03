#pragma once
// Sterownik Waveshare TVOC (SKU 30712), UART1, 115200, tryb aktywny.
// Ramka 11 B: FE 02 00 CO2_hi CO2_lo CH2O_hi CH2O_lo TVOC_hi TVOC_lo SUM 16
//   CO2 [ppm], CH2O [ppb], TVOC [ppm] = (hi<<8|lo)/1000, poziom = bajt 1 (1..4)
#include <Arduino.h>

struct TvocData {
    float co2;      // ppm
    float ch2o;     // ppb
    float tvoc;     // ppm
    int   level;    // 1=swietne 2=dobre 3=srednie 4=slabe
    bool  valid;
    uint32_t lastMs;
};

void  tvoc_begin();
void  tvoc_loop();
TvocData tvoc_get();
