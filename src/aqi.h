#pragma once
#include <stdint.h>
// Indeks jakosci powietrza wewnetrznego (IAQ), 0-500.
// Wagi: PM2.5 (duza) > PM10 i CO2 (srednie) > TVOC (mala).
// Formaldehyd i czujniki CO (MQ-7/MiCS) NIE wchodza do IAQ - pokazywane osobno.
struct IaqResult {
    int score;   // 0-500; -1 = brak jakichkolwiek danych
    int cause;   // 0=PM2.5, 1=PM10, 2=CO2, 3=TVOC; -1 = brak
};
IaqResult iaq_compute(float pm25, float pm10, float co2, float tvoc);

int aqi_from_pm25(float pm25);      // (zachowane, EPA PM2.5)
const char* aqi_label(int v);       // etykieta kategorii IAQ
uint32_t aqi_color(int v);          // 0xRRGGBB
