#include "drv_ads1115.h"
#include "pins.h"
#include "config.h"
#include "nvs_store.h"
#include "syslog.h"
#include <Adafruit_ADS1X15.h>
#include <Wire.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static Adafruit_ADS1115* ads = nullptr;
static MicsData s_data;

static bool s_present = false;
static uint32_t s_lastScan = 0;
static uint32_t s_startMs = 0;   // poczatek pracy (wygrzewanie od tego momentu)

// Mediana napiecia MiCS: probka co 0.5 s, okno 10 probek -> wynik co ~5 s.
#define MICS_SAMPLE_MS   500
#define MICS_MEDIAN_WIN  10
static float s_voutWin[MICS_MEDIAN_WIN];
static int   s_voutN = 0;
static uint32_t s_lastSample = 0;

static Mq7Data  s_mq7;
static float    s_mq7Win[MICS_MEDIAN_WIN];
static int      s_mq7N = 0;
static uint32_t s_mq7LastGoodMs = 0;

// Stan grzalki sterowanej firmware (PIN_MICS_EN >= 0). EN aktywny NISKI.
static bool s_heaterOn = true;

void ads_set_heater(bool on) {
    s_heaterOn = on;
#if PIN_MICS_EN >= 0
    pinMode(PIN_MICS_EN, OUTPUT);
    digitalWrite(PIN_MICS_EN, on ? LOW : HIGH);   // LOW = grzalka ON
    LOG_I("MiCS5524: grzalka %s (EN=%s, GPIO%d)", on ? "WLACZONA" : "WYLACZONA",
          on ? "LOW" : "HIGH", PIN_MICS_EN);
#endif
}

bool ads_heater_on() {
#if PIN_MICS_EN >= 0
    return s_heaterOn;
#else
    return true;   // EN na GND na stale -> grzalka zawsze wlaczona
#endif
}

static int mics_cmp(const void* a, const void* b) {
    float x = *(const float*)a, y = *(const float*)b;
    return (x > y) - (x < y);
}
static float mics_median(float* w, int n) {
    if (n <= 0) return NAN;
    float t[MICS_MEDIAN_WIN];
    memcpy(t, w, n * sizeof(float));
    qsort(t, n, sizeof(float), mics_cmp);
    return (n & 1) ? t[n / 2] : (t[n / 2 - 1] + t[n / 2]) * 0.5f;
}

void ads_begin() {
    Wire.begin(PIN_SDA, PIN_SCL);
#if PIN_MICS_EN >= 0
    pinMode(PIN_MICS_EN, OUTPUT);
    digitalWrite(PIN_MICS_EN, LOW);   // EN aktywny NISKIM = grzalka + czujnik WLACZONE
    s_heaterOn = true;
    LOG_I("MiCS5524: EN=LOW (GPIO%d) = grzalka wlaczona", PIN_MICS_EN);
#endif
    ads = new Adafruit_ADS1115();
    if (ads->begin(ADS1115_I2C_ADDR, &Wire)) {
        ads->setGain(GAIN_TWOTHIRDS);        // +/-6.144 V (pokrywa 0-5 V z MiCS)
        ads->setDataRate(RATE_ADS1115_128SPS);  // mniej szumu, do pomiaru gazu
        s_present = true;
        s_startMs = millis();
        LOG_I("ADS1115: OK (0x%02X, gain +/-6.144 V)", ADS1115_I2C_ADDR);
    } else {
        s_present = false;
        LOG_E("ADS1115: brak odpowiedzi na 0x%02X (SDA=%d, SCL=%d)",
              ADS1115_I2C_ADDR, PIN_SDA, PIN_SCL);
    }
}

// Ile czasu trzymac ostatni dobry odczyt zanim pokazemy "brak danych".
// Pojedynczy blad I2C (NACK) nie powinien powodowac mrugania kafelka.
#define ADS_STALE_MS        10000UL
// Po ilu kolejnych bledach uznajemy magistrale za zawieszona i ja resetujemy.
#define ADS_FAIL_RESET      8
// Timeout konwersji (128 SPS = ~8 ms; 25 ms to bezpieczny zapas).
#define ADS_CONV_TIMEOUT_MS 25

static uint32_t s_lastGoodMs = 0;
static int      s_failStreak = 0;

// Odczyta kanal single-ended z timeoutem. Bibliotekowa funkcja
// readADC_SingleEnded() ma w srodku NIESKONCZONA petle while(!conversionComplete()),
// ktora przy zablokowanej magistrali I2C zawiesza caly program. Tu robimy to
// samo, ale z timeoutem - po jego przekroczeniu zwracamy -1 (blad).
static int16_t ads_read_ch(uint8_t ch, uint32_t timeoutMs) {
    ads->startADCReading(MUX_BY_CHANNEL[ch], /*continuous=*/false);
    uint32_t t0 = millis();
    while (!ads->conversionComplete()) {
        if (millis() - t0 > timeoutMs) return -1;
        delay(1);
    }
    int16_t raw = ads->getLastConversionResults();
    // -1 = odczyt zwrocil 0xFFFF (bledny odczyt I2C, NACK) - traktujemy jako blad.
    return (raw == -1) ? -1 : raw;
}

void ads_loop() {
    if (!ads || !s_present) {
        // proby ponownego wykrycia co 5 s
        if (millis() - s_lastScan > 5000) {
            s_lastScan = millis();
            if (ads && ads->begin(ADS1115_I2C_ADDR, &Wire)) {
                ads->setGain(GAIN_TWOTHIRDS);
                ads->setDataRate(RATE_ADS1115_128SPS);
                s_present = true;
                s_startMs = millis();
                LOG_I("ADS1115: powrocil do pracy");
            }
        }
        return;
    }

    // MiCS nie wymaga szybkiego odczytu - probka co 500 ms (mniej szumu, mniej I2C).
    uint32_t nowMs = millis();
    if (nowMs - s_lastSample < MICS_SAMPLE_MS) return;
    s_lastSample = nowMs;

    int16_t raw = ads_read_ch(MICS_ADC_CHANNEL, ADS_CONV_TIMEOUT_MS);
    if (raw == -1) {
        // przejsciowe zaklocenie I2C - jedna proba ponowna
        raw = ads_read_ch(MICS_ADC_CHANNEL, ADS_CONV_TIMEOUT_MS);
    }

    if (raw != -1) {
        float adcV = ads->computeVolts(raw);           // napiecie na wejsciu ADC
        float vout = adcV * MICS_R_DIVIDER;            // odtworzenie napiecia czujnika
        s_data.adcRawV = adcV;

        // Mediana z ostatnich 10 probek (okno ~5 s) -> stabilny odczyt bez skokow.
        if (s_voutN < MICS_MEDIAN_WIN) s_voutN++;
        memmove(s_voutWin + 1, s_voutWin, (MICS_MEDIAN_WIN - 1) * sizeof(float));
        s_voutWin[0] = vout;
        s_data.vout = mics_median(s_voutWin, s_voutN);

        s_data.valid = true;
        s_data.lastMs = nowMs;
        s_data.warmup = (nowMs - s_startMs) < MICS_WARMUP_MS;
        s_lastGoodMs = nowMs;
        s_failStreak = 0;

        // Rs/R0 = ((Vcc/Vout)-1) / ((Vcc/V0)-1)  (V0 = napiecie w czystym powietrzu)
        if (cfgMicsR0 > 0.005f && s_data.vout > 0.003f) {
            float num = (MICS_VCC / s_data.vout) - 1.0f;
            float den = (MICS_VCC / cfgMicsR0) - 1.0f;
            s_data.rsR0 = (den > 0.0f) ? (num / den) : NAN;

            // Szacunkowy ekwiwalent CO [ppm] z krzywej (nieselektywny - patrz config.h)
            if (s_data.rsR0 > 0.02f && !isnan(s_data.rsR0)) {
                // c = (A/RsR0)^B - A^B ; odejmujemy A^B, zeby przy
                // Rs/R0=1.0 (czyste powietrze) wynik byl 0 ppm, a nie ~17.
                float c = powf(MICS_CO_A / s_data.rsR0, MICS_CO_B)
                        - powf(MICS_CO_A, MICS_CO_B);
                if (c > 1000.0f) c = 1000.0f;
                if (c < 0.0f)    c = 0.0f;
                s_data.coEquiv = c;
            } else {
                s_data.coEquiv = 1000.0f;
            }
        } else {
            s_data.rsR0 = NAN;
            s_data.coEquiv = NAN;
        }
    } else {
        // Blad odczytu MiCS. Trzymamy ostatnia dobra wartosc przez ADS_STALE_MS,
        // dopiero po tym czasie oznaczamy "brak danych" (koniec mrugania).
        s_failStreak++;
        if (s_lastGoodMs != 0 && (nowMs - s_lastGoodMs) > ADS_STALE_MS) {
            s_data.valid = false;
        }
        if (s_failStreak >= ADS_FAIL_RESET) {
            LOG_W("ADS1115: %d kolejnych bledow I2C - reset magistrali", s_failStreak);
            s_failStreak = 0;
            s_present = false;
            s_lastScan = nowMs;
            Wire.end();
            Wire.begin(PIN_SDA, PIN_SCL);
        }
    }

    // --- MQ-7 (AIN2) ---
    int16_t raw2 = ads_read_ch(MQ7_ADC_CHANNEL, ADS_CONV_TIMEOUT_MS);
    if (raw2 == -1) {
        // przejsciowe zaklocenie I2C - jedna proba ponowna
        raw2 = ads_read_ch(MQ7_ADC_CHANNEL, ADS_CONV_TIMEOUT_MS);
    }
    if (raw2 != -1) {
        float adcV2 = ads->computeVolts(raw2);
        float vout2  = adcV2;                        // A0 wprost do AIN2 (0-5 V)
        s_mq7.adcRawV = adcV2;

        // Mediana jak dla MiCS (okno ~5 s).
        if (s_mq7N < MICS_MEDIAN_WIN) s_mq7N++;
        memmove(s_mq7Win + 1, s_mq7Win, (MICS_MEDIAN_WIN - 1) * sizeof(float));
        s_mq7Win[0] = vout2;
        s_mq7.vout = mics_median(s_mq7Win, s_mq7N);

        s_mq7.valid  = true;
        s_mq7.lastMs = nowMs;
        s_mq7.warmup = (nowMs - s_startMs) < MQ7_WARMUP_MS;
        s_mq7LastGoodMs = nowMs;

        // Rs/R0 jak dla MiCS, z wlasnym R0 (napiecie w czystym powietrzu).
        if (cfgMq7R0 > 0.005f && s_mq7.vout > 0.003f) {
            float num = (MICS_VCC / s_mq7.vout) - 1.0f;
            float den = (MICS_VCC / cfgMq7R0) - 1.0f;
            s_mq7.rsR0 = (den > 0.0f) ? (num / den) : NAN;
            if (s_mq7.rsR0 > 0.02f && !isnan(s_mq7.rsR0)) {
                // ppm = A * (Rs/R0)^(-B) - A ; odejmujemy A, zeby przy
                // Rs/R0=1.0 (czyste powietrze) wynik byl 0 ppm, a nie 100.
                float c = MQ7_CO_A * (powf(s_mq7.rsR0, -MQ7_CO_B) - 1.0f);
                if (c > 1000.0f) c = 1000.0f;
                if (c < 0.0f)    c = 0.0f;
                s_mq7.coPpm = c;
            } else {
                s_mq7.coPpm = 1000.0f;
            }
        } else {
            s_mq7.rsR0  = NAN;
            s_mq7.coPpm = NAN;
        }
    } else {
        if (s_mq7LastGoodMs != 0 && (nowMs - s_mq7LastGoodMs) > ADS_STALE_MS) {
            s_mq7.valid = false;
        }
    }
}

MicsData ads_mics_get() { return s_data; }
Mq7Data  ads_mq7_get()  { return s_mq7; }
bool ads_present() { return s_present; }
