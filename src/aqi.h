#pragma once
#include <stdint.h>
// Indeks jakosci powietrza wewnetrznego (IAQ) liczony jako "najgorszy"
// z parametrow PM2.5/PM10/CO2/TVOC/CH2O (0-500, jak skala na pulpicie).
struct IaqResult {
    int score;   // 0-500; -1 = brak jakichkolwiek danych
    int cause;   // 0=PM2.5, 1=PM10, 2=CO2, 3=TVOC, 4=CH2O; -1 = brak
};
IaqResult iaq_compute(float pm25, float pm10, float co2, float tvoc, float ch2o);

int aqi_from_pm25(float pm25);      // (zachowane, EPA PM2.5)
const char* aqi_label(int v);       // etykieta kategorii IAQ
uint32_t aqi_color(int v);          // 0xRRGGBB
