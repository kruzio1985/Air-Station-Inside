#include "agg.h"
#include "syslog.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <esp_heap_caps.h>

#define PM_MEDIAN_WIN 30
#define STALE_MS      15000   // po tym czasie PM uznajemy za nieaktualny

// --- Okno mediany PM ---
static float s_pm1Win[PM_MEDIAN_WIN];
static float s_pm25Win[PM_MEDIAN_WIN];
static float s_pm10Win[PM_MEDIAN_WIN];
static int   s_pmWinN = 0;
static float s_pm1F = NAN, s_pm25F = NAN, s_pm10F = NAN;
static uint32_t s_lastPmMs = 0;

// --- Akumulatory biezacej minuty ---
static double   s_sumPm1, s_sumPm25, s_sumPm10;
static double   s_sumCo2, s_sumTvoc, s_sumCh2o, s_sumTemp, s_sumHum;
static uint32_t s_cntPm1, s_cntPm25, s_cntPm10;
static uint32_t s_cntCo2, s_cntTvoc, s_cntCh2o, s_cntTemp, s_cntHum;

static uint32_t s_lastTick = 0;
static uint32_t s_minStart = 0;

// --- Ring buffer 24 h ---
static MinuteRec* s_recs = nullptr;
static uint32_t s_head = 0;      // indeks najstarszego rekordu (gdy pelny)
static uint32_t s_count = 0;     // liczba rekordow w buforze
static uint32_t s_written = 0;   // ile rekordow juz zapisano na SD

// --- Sumy 24 h (tylko pola wystawiane jako srednie) ---
static double   s_24pm25, s_24temp, s_24hum;
static uint32_t s_24n_pm25, s_24n_temp, s_24n_hum;

static int cmpf(const void* a, const void* b) {
    float x = *(const float*)a, y = *(const float*)b;
    return (x > y) - (x < y);
}

static float median(float* w, int n) {
    if (n <= 0) return NAN;
    float t[PM_MEDIAN_WIN];
    memcpy(t, w, n * sizeof(float));
    qsort(t, n, sizeof(float), cmpf);
    if (n & 1) return t[n / 2];
    return (t[n / 2 - 1] + t[n / 2]) * 0.5f;
}

static uint32_t phys(uint32_t i) {   // i = indeks logiczny (0..count-1, od najstarszego)
    if (s_count < AGG_CAP) return i;
    return (s_head + i) % AGG_CAP;
}

static void add24(const MinuteRec& r) {
    if (!isnan(r.pm25)) { s_24pm25 += r.pm25; s_24n_pm25++; }
    if (!isnan(r.temp)) { s_24temp += r.temp; s_24n_temp++; }
    if (!isnan(r.hum))  { s_24hum  += r.hum;  s_24n_hum++;  }
}

static void sub24(const MinuteRec& r) {
    if (!isnan(r.pm25)) { s_24pm25 -= r.pm25; s_24n_pm25--; }
    if (!isnan(r.temp)) { s_24temp -= r.temp; s_24n_temp--; }
    if (!isnan(r.hum))  { s_24hum  -= r.hum;  s_24n_hum--;  }
}

static void finalize_minute(uint32_t ts) {
    MinuteRec r;
    r.ts   = ts;
    r.pm1  = s_cntPm1  ? (float)(s_sumPm1  / s_cntPm1)  : NAN;
    r.pm25 = s_cntPm25 ? (float)(s_sumPm25 / s_cntPm25) : NAN;
    r.pm10 = s_cntPm10 ? (float)(s_sumPm10 / s_cntPm10) : NAN;
    r.co2  = s_cntCo2  ? (float)(s_sumCo2  / s_cntCo2)  : NAN;
    r.tvoc = s_cntTvoc ? (float)(s_sumTvoc / s_cntTvoc) : NAN;
    r.ch2o = s_cntCh2o ? (float)(s_sumCh2o / s_cntCh2o) : NAN;
    r.temp = s_cntTemp ? (float)(s_sumTemp / s_cntTemp) : NAN;
    r.hum  = s_cntHum  ? (float)(s_sumHum  / s_cntHum)  : NAN;

    if (s_count < AGG_CAP) {
        s_recs[s_count] = r;
        s_count++;
    } else {
        sub24(s_recs[s_head]);
        s_recs[s_head] = r;
        s_head = (s_head + 1) % AGG_CAP;
    }
    add24(r);

    s_sumPm1 = s_sumPm25 = s_sumPm10 = 0;
    s_sumCo2 = s_sumTvoc = s_sumCh2o = s_sumTemp = s_sumHum = 0;
    s_cntPm1 = s_cntPm25 = s_cntPm10 = 0;
    s_cntCo2 = s_cntTvoc = s_cntCh2o = s_cntTemp = s_cntHum = 0;
}

void agg_begin() {
    s_recs = (MinuteRec*)heap_caps_malloc(AGG_CAP * sizeof(MinuteRec), MALLOC_CAP_SPIRAM);
    bool inPsram = (s_recs != nullptr);
    if (!s_recs) s_recs = (MinuteRec*)malloc(AGG_CAP * sizeof(MinuteRec));
    s_head = 0; s_count = 0; s_written = 0;
    s_pmWinN = 0; s_lastPmMs = 0;
    s_pm1F = s_pm25F = s_pm10F = NAN;
    s_sumPm1 = s_sumPm25 = s_sumPm10 = 0;
    s_sumCo2 = s_sumTvoc = s_sumCh2o = s_sumTemp = s_sumHum = 0;
    s_cntPm1 = s_cntPm25 = s_cntPm10 = 0;
    s_cntCo2 = s_cntTvoc = s_cntCh2o = s_cntTemp = s_cntHum = 0;
    s_24pm25 = s_24temp = s_24hum = 0;
    s_24n_pm25 = s_24n_temp = s_24n_hum = 0;
    s_lastTick = 0; s_minStart = 0;
    if (s_recs) {
        LOG_I("Agregacja: ring buffer 24h w %s (%.1f kB)",
              inPsram ? "PSRAM" : "SRAM",
              (AGG_CAP * sizeof(MinuteRec)) / 1024.0f);
    } else {
        LOG_E("Agregacja: brak pamieci na ring buffer 24h!");
    }
}

void agg_loop(uint32_t nowMs, uint32_t nowUnix,
              float pm1, float pm25, float pm10, bool pmFresh,
              float co2, float tvoc, float ch2o, bool tvocFresh,
              float temp, float hum, bool dhFresh) {
    if (!s_recs) return;
    if (s_lastTick == 0) { s_lastTick = nowMs; s_minStart = nowMs; }

    // --- mediana PM (dodawaj tylko swieze probki) ---
    if (pmFresh) {
        s_lastPmMs = nowMs;
        if (s_pmWinN < PM_MEDIAN_WIN) s_pmWinN++;
        memmove(s_pm1Win  + 1, s_pm1Win,  (PM_MEDIAN_WIN - 1) * sizeof(float));
        memmove(s_pm25Win + 1, s_pm25Win, (PM_MEDIAN_WIN - 1) * sizeof(float));
        memmove(s_pm10Win + 1, s_pm10Win, (PM_MEDIAN_WIN - 1) * sizeof(float));
        s_pm1Win[0] = pm1; s_pm25Win[0] = pm25; s_pm10Win[0] = pm10;
    }
    s_pm1F  = median(s_pm1Win,  s_pmWinN);
    s_pm25F = median(s_pm25Win, s_pmWinN);
    s_pm10F = median(s_pm10Win, s_pmWinN);
    bool pmStale = (nowMs - s_lastPmMs) > STALE_MS;

    // --- tick 1 s: akumulacja do minuty ---
    if (nowMs - s_lastTick < 1000) return;
    s_lastTick = nowMs;

    if (!pmStale && !isnan(s_pm1F))  { s_sumPm1  += s_pm1F;  s_cntPm1++;  }
    if (!pmStale && !isnan(s_pm25F)) { s_sumPm25 += s_pm25F; s_cntPm25++; }
    if (!pmStale && !isnan(s_pm10F)) { s_sumPm10 += s_pm10F; s_cntPm10++; }
    if (tvocFresh) { s_sumCo2 += co2; s_cntCo2++; s_sumTvoc += tvoc; s_cntTvoc++; s_sumCh2o += ch2o; s_cntCh2o++; }
    if (dhFresh)   { s_sumTemp += temp; s_cntTemp++; s_sumHum += hum; s_cntHum++; }

    if (nowMs - s_minStart >= 60000) {
        finalize_minute(nowUnix);
        s_minStart = nowMs;
    }
}

bool agg_pm_valid() { return s_pmWinN > 0 && !isnan(s_pm25F) && (millis() - s_lastPmMs) <= STALE_MS; }
float agg_pm1()  { return s_pm1F; }
float agg_pm25() { return s_pm25F; }
float agg_pm10() { return s_pm10F; }

bool agg_has_minutes() { return s_count > 0; }

float agg_pm25_1min() {
    if (s_count == 0) return NAN;
    return s_recs[phys(s_count - 1)].pm25;
}
float agg_pm25_5min() {
    int n = s_count < 5 ? (int)s_count : 5;
    if (n <= 0) return NAN;
    double sum = 0; int c = 0;
    for (int i = (int)s_count - n; i < (int)s_count; i++) {
        float v = s_recs[phys(i)].pm25;
        if (!isnan(v)) { sum += v; c++; }
    }
    return c ? (float)(sum / c) : NAN;
}
float agg_pm25_24h() { return s_24n_pm25 ? (float)(s_24pm25 / s_24n_pm25) : NAN; }
float agg_temp_24h() { return s_24n_temp ? (float)(s_24temp / s_24n_temp) : NAN; }
float agg_hum_24h()  { return s_24n_hum  ? (float)(s_24hum  / s_24n_hum)  : NAN; }

uint32_t agg_pending() { return s_count - s_written; }
bool agg_pending_get(uint32_t i, MinuteRec& out) {
    if (i >= agg_pending()) return false;
    out = s_recs[phys(s_written + i)];
    return true;
}
void agg_pending_mark(uint32_t n) {
    if (n > agg_pending()) n = agg_pending();
    s_written += n;
}
void agg_restore_rec(const MinuteRec& r) {
    if (!s_recs) return;
    if (s_count < AGG_CAP) { s_recs[s_count] = r; s_count++; }
    else { sub24(s_recs[s_head]); s_recs[s_head] = r; s_head = (s_head + 1) % AGG_CAP; }
    add24(r);
    s_written = s_count;
}
void agg_restore_done() {
    s_written = s_count;
    s_minStart = millis();
}

uint32_t agg_count() { return s_count; }
bool agg_get(uint32_t i, MinuteRec& out) {
    if (i >= s_count) return false;
    out = s_recs[phys(i)];
    return true;
}
