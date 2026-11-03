#pragma once
// Agregacja pomiarow: mediana PM (okno 30 s), rekordy minutowe,
// ring buffer 24 h (1440 minut), srednie 1/5/24 h i kolejka rekordow do SD.
#include <Arduino.h>

#define AGG_CAP 1440   // 1440 minut = 24 h

struct MinuteRec {
    uint32_t ts;            // unix (0 = brak czasu)
    float pm1, pm25, pm10;  // ug/m3
    float co2, tvoc, ch2o;  // ppm / ppm / ppb
    float temp, hum;        // degC / %
};

void agg_begin();

// Wywolywac w loop(). "Fresh" = czy w tej iteracji jest swiezy odczyt sensora.
void agg_loop(uint32_t nowMs, uint32_t nowUnix,
              float pm1, float pm25, float pm10, bool pmFresh,
              float co2, float tvoc, float ch2o, bool tvocFresh,
              float temp, float hum, bool dhFresh);

// Wartosci PM po medianie (do wyswietlania / kanalow)
bool  agg_pm_valid();
float agg_pm1();
float agg_pm25();
float agg_pm10();

// Srednie
float agg_pm25_1min();
float agg_pm25_5min();
float agg_pm25_24h();
float agg_temp_24h();
float agg_hum_24h();
bool  agg_has_minutes();

// SD: rekordy minutowe czekajace na zapis
uint32_t agg_pending();
bool     agg_pending_get(uint32_t i, MinuteRec& out);   // i = 0..pending-1 (od najstarszego)
void     agg_pending_mark(uint32_t n);                  // potwierdzenie zapisu n rekordow
void     agg_restore_rec(const MinuteRec& r);           // odtworzenie rekordu z CSV (juz na SD)
void     agg_restore_done();
uint32_t agg_count();
bool     agg_get(uint32_t i, MinuteRec& out);           // i = 0..count-1 (od najstarszego)
