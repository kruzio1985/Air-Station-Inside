#pragma once
// Sterownik ADS1115 (I2C 0x48) z czujnikiem gazow MiCS5524 na AIN0.
// Zasilanie ADS1115 = 5 V, wejscie 0-5 V (bez dzielnika). Gain +/-6.144 V.
#include <Arduino.h>

struct MicsData {
    float vout;     // napiecie wyjscia czujnika [V] (po odtworzeniu z dzielnika)
    float rsR0;     // stosunek Rs/R0 (1.0 = czyste powietrze); NaN gdy nie skalibrowano
    float adcRawV;  // surowe napiecie na wejsciu ADC [V]
    float coEquiv;  // ekwiwalent CO [ppm] (przyblizony, nieselektywny)
    bool  warmup;   // true = czujnik sie jeszcze wygrzewa (nie ufac odczytom)
    bool  valid;
    uint32_t lastMs;
};

// MQ-7 (CO) na kanale ADS1115 (AIN2).
struct Mq7Data {
    float vout;     // napiecie wyjscia czujnika [V]
    float rsR0;     // stosunek Rs/R0; NaN gdy nie skalibrowano
    float adcRawV;  // surowe napiecie na wejsciu ADC [V]
    float coPpm;    // CO [ppm] (orientacyjny, z krzywej MQ-7)
    bool  warmup;   // true = przed uplywem MQ7_WARMUP_MS
    bool  valid;
    uint32_t lastMs;
};

void  ads_begin();
void  ads_loop();
MicsData ads_mics_get();
Mq7Data  ads_mq7_get();
bool  ads_present();
void  ads_set_heater(bool on);   // true = grzalka ON (EN=LOW); tylko gdy PIN_MICS_EN >= 0
bool  ads_heater_on();           // stan grzalki sterowanej firmware
