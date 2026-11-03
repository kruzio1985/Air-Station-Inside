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
// Kategorie: 50=Dobra, 100=Umiarkowana, 150=Zla, 200=Bardzo zla, 300=Niebezpieczna.
// Progi zgodne z kolorami kafelkow (skala z uwzglednieniem bledu czujnikow).
static const float PM25_BP[][2] = {{0,0},{10,50},{35,100},{50,150},{75,200},{150,300},{500,500}};
static const float PM10_BP[][2] = {{0,0},{20,50},{45,100},{75,150},{100,200},{200,300},{500,500}};
static const float CO2_BP[][2]  = {{0,0},{600,50},{1000,100},{1500,150},{2000,200},{3000,300},{5000,500}};
static const float TVOC_BP[][2] = {{0,0},{0.3f,50},{0.5f,100},{1.0f,150},{2.0f,200},{3.0f,300},{5.0f,500}};
static const float CH2O_BP[][2] = {{0,0},{20,50},{40,100},{80,150},{120,200},{200,300},{500,500}};

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

IaqResult iaq_compute(float pm25, float pm10, float co2, float tvoc, float ch2o) {
    struct { float v; const float (*bp)[2]; int n; int cause; } pars[5] = {
        {pm25, PM25_BP, 7, 0},
        {pm10, PM10_BP, 7, 1},
        {co2,  CO2_BP,  7, 2},
        {tvoc, TVOC_BP, 7, 3},
        {ch2o, CH2O_BP, 7, 4},
    };
    IaqResult r;
    r.score = -1;
    r.cause = -1;
    for (auto& p : pars) {
        int s = sub_index(p.v, p.bp, p.n);
        if (s > r.score) { r.score = s; r.cause = p.cause; }
    }
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
    if (aqi <= 50)    return 0x00C853;   // zielony
    if (aqi <= 100)   return 0xFFD600;   // zolty
    if (aqi <= 150)   return 0xFF9100;   // pomaranczowy
    if (aqi <= 200)   return 0xE53935;   // czerwony
    if (aqi <= 300)   return 0x8E24AA;   // fioletowy
    return 0x7B1F1F;                     // bordo
}
