#include "drv_dht.h"
#include "pins.h"
#include "config.h"
#include "syslog.h"
#include <DHTesp.h>
#include <math.h>

static DHTesp dht;
static DhtData s_data;

static uint32_t s_lastRead = 0;

void dht_begin() {
    if (PIN_DHT <= 0) {
        LOG_W("DHT11: wylaczony (brak pinu)");
        return;
    }
    dht.setup(PIN_DHT, DHTesp::DHT11);
    LOG_I("DHT11: GPIO%d (odczyt co ~2 s)", PIN_DHT);
}

void dht_loop() {
    if (PIN_DHT <= 0) return;
    uint32_t now = millis();
    if (now - s_lastRead < 2000) return;
    s_lastRead = now;

    TempAndHumidity th = dht.getTempAndHumidity();
    if (isnan(th.temperature) || isnan(th.humidity)) {
        // Blad odczytu (DHT11 potrafi zgubic bit) - zostaje ostatnia dobra wartosc.
        return;
    }
    if (th.temperature < -40.0f || th.temperature > 80.0f ||
        th.humidity < 0.0f || th.humidity > 100.0f) {
        return;   // sanity check
    }

    s_data.temp = th.temperature;
    s_data.hum  = th.humidity;
    s_data.valid = true;
    s_data.lastMs = now;

    static uint32_t lastLog = 0;
    if (now - lastLog > 30000) {
        lastLog = now;
        LOG_I("DHT11: %.1f C, %.1f %% RH", s_data.temp, s_data.hum);
    }
}

DhtData dht_get() { return s_data; }
