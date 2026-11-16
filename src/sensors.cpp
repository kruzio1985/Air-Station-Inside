#include "sensors.h"
#include "app.h"
#include "pins.h"
#include "config.h"
#include "syslog.h"
#include "drv_pms7003.h"
#include "drv_tvoc.h"
#include "drv_ads1115.h"
#include "drv_dht.h"
#include "drv_sht31.h"
#include "drv_ha.h"
#include "aqi.h"
#include "agg.h"
#include <time.h>

// --- Stan globalny -------------------------------------------------------------
Channel channels[32];
BoxMeta boxMeta[BOX_COUNT];
const int channelCount = 26;   // liczba kanalow z enum ChanIdx
Box boxes[BOX_COUNT];

static uint32_t s_motionLastMs = 0;
static uint32_t s_pirLastAccept = 0;       // czas ostatniego uznanego zbocza (debounce)
static uint32_t s_pirQuietSince = 0;       // jak dlugo PIR jest w stanie "nieaktywnym"
static bool     s_pirActivePrev = false;   // poprzedni stan "aktywny" (po polaryzacji)

// Minimalny czas, przez ktory PIR musi byc w stanie "nieaktywnym" przed
// kolejnym uznanym zboczem. Odfiltrowuje ciagle re-triggery SR505.
#define PIR_REARM_MS  3000

void channels_init() {
    channels[CH_PM1_CF1]     = {"pm1",     "PM1.0",       "ug/m3", 0, 0, 1000, false, 0};
    channels[CH_PM25_CF1]    = {"pm25",    "PM2.5",       "ug/m3", 0, 0, 1000, false, 0};
    channels[CH_PM10_CF1]    = {"pm10",    "PM10",        "ug/m3", 0, 0, 1000, false, 0};
    channels[CH_AQI]         = {"aqi",     "IAQ",         "",      0, 0, 500,  false, 0};
    channels[CH_CO2]         = {"co2",     "CO2",         "ppm",   0, 400, 5000, false, 0};
    channels[CH_CH2O]        = {"ch2o",    "Formaldehyd", "ppb",   0, 0, 1000, false, 0};
    channels[CH_TVOC]        = {"tvoc",    "TVOC",        "ppm",   0, 0, 100,  false, 0};
    channels[CH_TVOC_LEVEL]  = {"tvoclvl", "TVOC poziom", "",      0, 1, 4,    false, 0};
    channels[CH_MICS_V]      = {"mics_v",  "MiCS5524 V", "V",     0, 0, 5,    false, 0};
    channels[CH_MICS_RSR0]   = {"mics_rs", "MiCS Rs/R0",  "",      0, 0, 10,   false, 0};
    channels[CH_MICS_CO]     = {"mics_co", "CO ekwiwalent","ppm",  0, 1, 1000, false, 0};
    channels[CH_MICS_WARMUP] = {"mics_wu", "MiCS wygrzewanie","",  0, 0, 1,    false, 0};
    channels[CH_MQ7_V]       = {"mq7_v",   "MQ-7 V",       "V",     0, 0, 5,    false, 0};
    channels[CH_MQ7_RS]      = {"mq7_rs",  "MQ-7 Rs/R0",   "",      0, 0, 100,  false, 0};
    channels[CH_MQ7_CO]      = {"mq7_co",  "MQ-7 CO",      "ppm",   0, 1, 1000, false, 0};
    channels[CH_MOTION]      = {"motion",  "Ruch",        "",      0, 0, 1,    false, 0};
    channels[CH_TEMP]        = {"temp",    "Temperatura", "degC",  0, -40, 80,  false, 0};
    channels[CH_HUM]         = {"hum",     "Wilgotnosc",  "%",     0, 0, 100,  false, 0};
    channels[CH_PM25_1M]     = {"pm25_1m", "PM2.5 1 min", "ug/m3", 0, 0, 1000, false, 0};
    channels[CH_PM25_5M]     = {"pm25_5m", "PM2.5 5 min", "ug/m3", 0, 0, 1000, false, 0};
    channels[CH_PM25_24H]    = {"pm25_24h","PM2.5 24 h",  "ug/m3", 0, 0, 1000, false, 0};
    channels[CH_TEMP_24H]    = {"temp_24h","Temp. 24 h",  "degC",  0, -40, 80, false, 0};
    channels[CH_HUM_24H]     = {"hum_24h", "Wilg. 24 h",  "%",     0, 0, 100,  false, 0};
    channels[CH_TEMP_EXT]    = {"tempext", "Temp. zewn.",  "degC",  0, -40, 80, false, 0};
    channels[CH_HUM_EXT]     = {"humext",  "Wilg. zewn.",  "%",     0, 0, 100, false, 0};
    channels[CH_RH_CORR]     = {"rhcorr",  "Korekcja RH",     "%",    0, 0, 100, false, 0};

    for (int i = 0; i < BOX_COUNT; i++) { boxes[i].temp = NAN; boxes[i].hum = NAN; boxes[i].online = false; boxes[i].lastMs = 0; }
}

Channel* channel_find(const char* id) {
    for (int i = 0; i < channelCount; i++)
        if (strcmp(channels[i].id, id) == 0) return &channels[i];
    return nullptr;
}

void sensors_begin() {
    channels_init();
    pms_begin();
    tvoc_begin();
    ads_begin();
    dht_begin();
    sht31_begin();
    ha_begin();
    agg_begin();
    if (PIN_PIR > 0) {
        pinMode(PIN_PIR, INPUT_PULLDOWN);
        LOG_I("PIR: pin %d (poling co ~20 ms, pull-down)", PIN_PIR);
    }
}

static void set_ch(int idx, float v, bool valid = true) {
    channels[idx].value = v;
    channels[idx].valid = valid;
    if (valid) channels[idx].lastMs = millis();
}

// Srednia TVOC z ostatnich 10 minut (rekordy minutowe z ring-buffera).
// Wygladza krotkie skoki (np. zapach z dywanu) - wg decyzji o wadze TVOC.
static float tvoc_avg_10min() {
    uint32_t n = agg_count();
    if (n == 0) return NAN;
    uint32_t start = n > 10 ? n - 10 : 0;
    float sum = 0;
    int cnt = 0;
    for (uint32_t i = start; i < n; i++) {
        MinuteRec r;
        if (agg_get(i, r) && !isnan(r.tvoc)) { sum += r.tvoc; cnt++; }
    }
    return cnt ? (sum / cnt) : NAN;
}

void sensors_loop() {
    pms_loop();
    tvoc_loop();
    ads_loop();
    dht_loop();
    sht31_loop();
    ha_loop();

    PmsData  p  = pms_get();
    TvocData t  = tvoc_get();
    MicsData m  = ads_mics_get();
    Mq7Data  mq = ads_mq7_get();
    DhtData dh  = dht_get();
    Sht31Data s1 = sht31_get(0);   // 0x44 - wewnetrzny (zamiast DHT11)
    Sht31Data s2 = sht31_get(1);   // 0x45 - zewnetrzny
    HaData  ha  = ha_get();

    uint32_t now  = millis();
    uint32_t unix = (uint32_t)time(nullptr);
    if (unix < 1600000000UL) unix = 0;   // NTP jeszcze nie zsynchronizowany

    bool pmFresh = p.valid  && (now - p.lastMs) < 5000;
    bool tvFresh = t.valid  && (now - t.lastMs) < 5000;
    bool dhFresh = dh.valid && (now - dh.lastMs) < 5000;
    bool s1Fresh = s1.valid && (now - s1.lastMs) < 5000;
    bool s2Fresh = s2.valid && (now - s2.lastMs) < 5000;

    // --- Temp/wilgotnosc: SHT31 z fallbackiem (DHT11 / HA) ---
    bool  iok = false; float it = NAN, ih = NAN;   // wewnetrzny
    if (s1.valid)      { iok = true; it = s1.temp; ih = s1.hum; }
    else if (dh.valid) { iok = true; it = dh.temp; ih = dh.hum; }
    bool  iFresh = s1.valid ? s1Fresh : dhFresh;

    bool  eok = false; float et = NAN, eh = NAN;   // zewnetrzny
    if (s2.valid)      { eok = true; et = s2.temp; eh = s2.hum; }
    else if (ha.valid) { eok = true; et = ha.temp; eh = ha.hum; }

    // --- Korekcja wilgotnosci PM ---
    // TYMCZASOWO WYLACZONA (diagnostyka): pokazujemy surowe wartosci PM.
    // Wartosci PM sa teraz uzywane BEZ zadnego przeliczania przez temp/RH.
    float pm1 = p.pm1, pm25 = p.pm25, pm10 = p.pm10;
    bool  rhCorrActive = false;
    set_ch(CH_RH_CORR, 0.0f, false);

    agg_loop(now, unix,
             pm1, pm25, pm10, pmFresh,
             t.co2, t.tvoc, t.ch2o, tvFresh,
             it, ih, iFresh);

    bool pmOk = agg_pm_valid();
    set_ch(CH_PM1_CF1,  pmOk ? agg_pm1()  : 0, pmOk);
    set_ch(CH_PM25_CF1, pmOk ? agg_pm25() : 0, pmOk);
    set_ch(CH_PM10_CF1, pmOk ? agg_pm10() : 0, pmOk);
    // IAQ z wagami: PM2.5 (duza) > PM10/CO2 (srednie) > TVOC (mala, srednia 10 min).
    // Formaldehyd i CO (MQ-7/MiCS) nie wplywaja na IAQ - pokazywane osobno.
    float tvoc10 = tvoc_avg_10min();
    if (isnan(tvoc10) && t.valid) tvoc10 = t.tvoc;   // brak historii - biezaca wartosc
    IaqResult iaq = iaq_compute(
        pmOk     ? agg_pm25() : NAN,
        pmOk     ? agg_pm10() : NAN,
        t.valid  ? t.co2      : NAN,
        t.valid  ? tvoc10     : NAN);
    set_ch(CH_AQI, iaq.score >= 0 ? iaq.score : 0, iaq.score >= 0);

    set_ch(CH_CO2,  t.co2,  t.valid);
    set_ch(CH_CH2O, t.ch2o, t.valid);
    set_ch(CH_TVOC, t.tvoc, t.valid);
    set_ch(CH_TVOC_LEVEL, t.level, t.valid);

    set_ch(CH_MICS_V,  m.vout, m.valid);
    set_ch(CH_MICS_RSR0, isnan(m.rsR0) ? 0 : m.rsR0, m.valid && !isnan(m.rsR0));
    bool micsCoOk = m.valid && !isnan(m.coEquiv) && !m.warmup;  // nie pokazuj CO przy rozgrzewaniu
    set_ch(CH_MICS_CO, micsCoOk ? m.coEquiv : 0, micsCoOk);
    set_ch(CH_MICS_WARMUP, m.warmup ? 1 : 0, m.valid);

    set_ch(CH_MQ7_V, mq.vout, mq.valid);
    set_ch(CH_MQ7_RS, isnan(mq.rsR0) ? 0 : mq.rsR0, mq.valid && !isnan(mq.rsR0));
    bool mq7CoOk = mq.valid && !isnan(mq.coPpm) && !mq.warmup;  // nie pokazuj CO przy wygrzewaniu
    set_ch(CH_MQ7_CO, mq7CoOk ? mq.coPpm : 0, mq7CoOk);

    set_ch(CH_TEMP, it, iok);
    set_ch(CH_HUM,  ih, iok);

    set_ch(CH_TEMP_EXT, et, eok);
    set_ch(CH_HUM_EXT,  eh, eok);

    // srednie kroczace (rekordy minutowe)
    bool hasMin = agg_has_minutes();
    set_ch(CH_PM25_1M,  hasMin ? agg_pm25_1min() : 0, hasMin && !isnan(agg_pm25_1min()));
    set_ch(CH_PM25_5M,  hasMin ? agg_pm25_5min() : 0, hasMin && !isnan(agg_pm25_5min()));
    set_ch(CH_PM25_24H, hasMin ? agg_pm25_24h()  : 0, hasMin && !isnan(agg_pm25_24h()));
    set_ch(CH_TEMP_24H, hasMin ? agg_temp_24h()  : 0, hasMin && !isnan(agg_temp_24h()));
    set_ch(CH_HUM_24H,  hasMin ? agg_hum_24h()   : 0, hasMin && !isnan(agg_hum_24h()));

    // --- Ruch lokalny (PIR) ---
    // "Aktywny" poziom zalezy od polaryzacji czujnika:
    //   cfgPirInverted=false -> aktywny HIGH (spoczynek LOW)
    //   cfgPirInverted=true  -> aktywny LOW  (spoczynek HIGH)
    // Ruch = zbocze na "aktywny" po min. PIR_REARM_MS ciszy + debounce.
    if (cfgPirEnabled && PIN_PIR > 0) {
        bool raw    = digitalRead(PIN_PIR);
        bool active = cfgPirInverted ? !raw : raw;
        uint32_t now = millis();
        if (active && !s_pirActivePrev) {
            if (now - s_pirQuietSince >= PIR_REARM_MS &&
                now - s_pirLastAccept >= (uint32_t)cfgPirDebounceMs) {
                s_pirLastAccept = now;
                note_motion_local();
            }
        }
        if (!active) s_pirQuietSince = now;
        s_pirActivePrev = active;
    }
    // okno "aktywnosci" - ruch liczy sie przez cfgPirHoldMs od ostatniego zdarzenia
    uint32_t hold = cfgPirHoldMs > 0 ? (uint32_t)cfgPirHoldMs : 60000;
    set_ch(CH_MOTION, (s_motionLastMs != 0 && (millis() - s_motionLastMs) < hold) ? 1 : 0);
}

bool motion_active() {
    uint32_t hold = cfgPirHoldMs > 0 ? (uint32_t)cfgPirHoldMs : 60000;
    return s_motionLastMs != 0 && (millis() - s_motionLastMs) < hold;
}

bool pir_raw_level() {
    return (PIN_PIR > 0) && (digitalRead(PIN_PIR) != 0);
}

void note_motion_from_mqtt() { s_motionLastMs = millis(); LOG_I("Ruch: zdarzenie z MQTT"); }
void note_motion_local()    { s_motionLastMs = millis(); LOG_I("Ruch: PIR (GPIO%d) wykryl ruch", PIN_PIR); }
