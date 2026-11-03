#include "drv_sht31.h"
#include "pins.h"
#include "syslog.h"
#include <Adafruit_SHT31.h>
#include <Wire.h>
#include <math.h>

#define SHT31_ADDR1       0x44   // wewnetrzny  (Wire1: SDA=47, SCL=40)
#define SHT31_ADDR2       0x44   // zewnetrzny  (Wire:  SDA=8,  SCL=9) - ten sam adres, inna magistrala
#define SHT31_INTERVAL_MS 2000   // SHT31 moze byc odczytywany co ~1 s; 2 s to spokojnie starczy

// Moduly SHT31 BEZ pinu ADDR maja staly adres 0x44, wiec nie moga byc razem
// na jednej magistrali. #1 idzie na Wire1 (47/40), #2 na Wire (8/9, obok
// ADS1115 0x48 - rozne adresy, brak konfliktu).
static Adafruit_SHT31 sht[2] = { Adafruit_SHT31(&Wire1), Adafruit_SHT31(&Wire) };
static Sht31Data s_data[2];
static bool      s_present[2]  = {false, false};
static uint32_t  s_lastRead[2] = {0, 0};

void sht31_begin() {
    Wire1.begin(PIN_SDA2, PIN_SCL2);
    if (sht[0].begin(SHT31_ADDR1)) {
        s_present[0] = true;
        LOG_I("SHT31 #1 (0x44, wewn., Wire1): OK");
    } else {
        LOG_W("SHT31 #1 (0x44, wewn.): brak (SDA=47/SCL=40)");
    }
    if (sht[1].begin(SHT31_ADDR2)) {
        s_present[1] = true;
        LOG_I("SHT31 #2 (0x44, zewn., Wire): OK");
    } else {
        LOG_W("SHT31 #2 (0x44, zewn.): brak (SDA=8/SCL=9)");
    }
}

void sht31_loop() {
    uint32_t now = millis();
    for (int i = 0; i < 2; i++) {
        uint8_t addr = (i == 0) ? SHT31_ADDR1 : SHT31_ADDR2;

        if (!s_present[i]) {
            // ponowna proba wykrycia co 5 s
            if (now - s_lastRead[i] < 5000) continue;
            s_lastRead[i] = now;
            if (sht[i].begin(addr)) {
                s_present[i] = true;
                LOG_I("SHT31 #%d (0x%02X): powrocil do pracy", i + 1, addr);
            }
            continue;
        }

        if (now - s_lastRead[i] < SHT31_INTERVAL_MS) continue;
        s_lastRead[i] = now;

        float t = sht[i].readTemperature();
        float h = sht[i].readHumidity();
        if (isnan(t) || isnan(h)) continue;                 // blad odczytu - zostaje ostatnia dobra wartosc
        if (t < -40.0f || t > 80.0f || h < 0.0f || h > 100.0f) continue;   // sanity check

        s_data[i].temp   = t;
        s_data[i].hum    = h;
        s_data[i].valid  = true;
        s_data[i].lastMs = now;
    }
}

Sht31Data sht31_get(int idx) {
    Sht31Data d;
    d.valid = false;
    d.temp = NAN;
    d.hum  = NAN;
    d.lastMs = 0;
    if (idx >= 0 && idx <= 1) return s_data[idx];
    return d;
}
