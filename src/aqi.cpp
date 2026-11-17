#include "aqi.h"
#include <math.h>

// Punkty przelamania EPA: [Clow, Chigh, Ilow, Ihigh]
static const float BP[][4] = {
    {0.0f,    12.0f,    0,   50},
    {12.1f,   35.4f,   51,  100},
    {35.5f,   55.4f,  101,  150},
    {55.5f,  150.4f,  151,  200},
    {150.5f, 250.4f,  201,  300},
    {250.5f, 350.4f,  301,  400},
    {350.5f, 500.4f,  401,  500},
};

int aqi_from_pm25(float pm25) {
    if (pm25 <= 0) return 0;
    for (auto& b : BP) {
        if (pm25 <= b[1]) {
            float clow = b[0], chigh = b[1], ilow = b[2], ihigh = b[3];
            return (int)lroundf(((ihigh - ilow) / (chigh - clow)) * (pm25 - clow) + ilow);
        }
    }
    return 500;
}

// --- IAQ: sub-indeks 0-500 dla jednego parametru (piecewise-linear) ----------
// Wagi zgodnie z decyzja: PM2.5 (duza) > PM10 i CO2 (srednie) > TVOC (mala).
// Formaldehyd (CH2O) i CO (MQ-7/MiCS) NIE wplywaja na IAQ - sa pokazywane osobno.
static const float PM25_BP[][2] = {{0,0},{10,50},{35,100},{50,150},{75,200},{150,300},{500,500}};
static const float PM10_BP[][2] = {{0,0},{20,50},{45,100},{75,150},{100,200},{200,300},{500,500}};
static const float CO2_BP[][2]  = {{0,0},{600,50},{1000,100},{1500,150},{2000,200},{3000,300},{5000,500}};
// TVOC: ponizej 0.5 ppm w ogole nie obniza IAQ. Powyzej - rosnaca korekta:
// 0.5->0, 1.0->30, 2.0->80, 3.0->150, 5.0->300.
static const float TVOC_BP[][2] = {{0.5f,0},{1.0f,30},{2.0f,80},{3.0f,150},{5.0f,300}};

// Wspolczynniki wplywu: PM10 i CO2 maja sredni wplyw (60% sub-indeksu).
static const float PM10_W = 0.6f;
static const float CO2_W  = 0.6f;

static int sub_index(float v, const float bp[][2], int n) {
    if (isnan(v) || v < 0) return -1;
    if (v <= bp[0][0]) return 0;
    for (int i = 0; i < n - 1; i++) {
        if (v <= bp[i + 1][0]) {
            float clow = bp[i][0], chigh = bp[i + 1][0];
            float ilow = bp[i][1], ihigh = bp[i + 1][1];
            return (int)lroundf(ilow + (ihigh - ilow) * (v - clow) / (chigh - clow));
        }
    }
    return (int)bp[n - 1][1];
}

IaqResult iaq_compute(float pm25, float pm10, float co2, float tvoc) {
    int s_pm25 = sub_index(pm25, PM25_BP, 7);
    int s_pm10 = (int)lroundf(sub_index(pm10, PM10_BP, 7) * PM10_W);
    int s_co2  = (int)lroundf(sub_index(co2,  CO2_BP,  7) * CO2_W);
    int s_tvoc = sub_index(tvoc, TVOC_BP, 5);

    IaqResult r;
    r.score = -1;
    r.cause = -1;
    int best = -1, cause = -1;
    if (s_pm25 > best) { best = s_pm25; cause = 0; }
    if (s_pm10 > best) { best = s_pm10; cause = 1; }
    if (s_co2  > best) { best = s_co2;  cause = 2; }
    if (s_tvoc > best) { best = s_tvoc; cause = 3; }
    if (best >= 0) { r.score = best; r.cause = cause; }
    return r;
}

const char* aqi_label(int v) {
    if (v <= 50)    return "Dobra";
    if (v <= 100)   return "Umiarkowana";
    if (v <= 150)   return "Zla (wrazliwe grupy)";
    if (v <= 200)   return "Zla";
    if (v <= 300)   return "Bardzo zla";
    return "Niebezpieczna";
}

uint32_t aqi_color(int aqi) {
    if (aqi <= 50)    return 0x00FF00;   // zielony (czysty)
    if (aqi <= 100)   return 0xFFD600;   // zolty
    if (aqi <= 150)   return 0xFF9100;   // pomaranczowy
    if (aqi <= 200)   return 0xE53935;   // czerwony
    if (aqi <= 300)   return 0x8E24AA;   // fioletowy
    return 0x7B1F1F;                     // bordo
}
