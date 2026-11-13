#include "drv_ha.h"
#include "config.h"
#include "nvs_store.h"
#include "syslog.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static HaData s_data;
static uint32_t s_lastPoll = 0;
static uint32_t s_lastErrLog = 0;

// Stan czujnika obecnosci (binary_sensor).
static bool     s_presence = false;
static bool     s_presenceValid = false;
static uint32_t s_lastPresencePoll = 0;

static bool fetch_state(const String& entity, float& out) {
    HTTPClient http;
    http.setTimeout(5000);
    String url = cfgHaUrl + "/api/states/" + entity;
    http.begin(url);
    http.addHeader("Authorization", "Bearer " + cfgHaToken);
    http.addHeader("Content-Type", "application/json");
    int code = http.GET();
    if (code != 200) {
        if (millis() - s_lastErrLog > 300000) {
            s_lastErrLog = millis();
            LOG_E("HA: GET %s -> HTTP %d", entity.c_str(), code);
        }
        http.end();
        return false;
    }
    String payload = http.getString();
    http.end();

    JsonDocument d;
    if (deserializeJson(d, payload)) return false;
    const char* st = d["state"] | "";
    if (st[0] == '\0') return false;
    char* end = nullptr;
    float v = strtof(st, &end);
    if (end == st) return false;   // "unavailable" / "unknown" / tekst
    out = v;
    return true;
}

void ha_begin() {
    if (cfgHaToken.length() == 0) {
        LOG_W("HA ext: wylaczony (brak tokenu)");
        return;
    }
    LOG_I("HA ext: %s / %s", cfgHaTempEntity.c_str(), cfgHaHumEntity.c_str());
    if (cfgPresenceLed) LOG_I("HA obecnosc: %s (LED wl/wyl)", cfgHaPresenceEntity.c_str());
}

// Odczyt binary_sensor: zwraca true gdy state == "on".
static bool fetch_bool_state(const String& entity, bool& out) {
    HTTPClient http;
    http.setTimeout(5000);
    String url = cfgHaUrl + "/api/states/" + entity;
    http.begin(url);
    http.addHeader("Authorization", "Bearer " + cfgHaToken);
    http.addHeader("Content-Type", "application/json");
    int code = http.GET();
    if (code != 200) {
        if (millis() - s_lastErrLog > 300000) {
            s_lastErrLog = millis();
            LOG_E("HA: GET %s -> HTTP %d", entity.c_str(), code);
        }
        http.end();
        return false;
    }
    String payload = http.getString();
    http.end();

    JsonDocument d;
    if (deserializeJson(d, payload)) return false;
    const char* st = d["state"] | "";
    if (st[0] == '\0') return false;
    if (strcmp(st, "on") == 0)  { out = true;  return true; }
    if (strcmp(st, "off") == 0) { out = false; return true; }
    return false;   // unavailable / unknown
}

void ha_loop() {
    if (cfgHaToken.length() == 0) return;
    uint32_t now = millis();

    // Obecnosc: osobny, szybszy odczyt (10 s) - niezalezny od temp/wilg.
    if (cfgPresenceLed && cfgHaPresenceEntity.length() &&
        now - s_lastPresencePoll >= HA_PRESENCE_POLL_MS) {
        s_lastPresencePoll = now;
        bool p;
        if (fetch_bool_state(cfgHaPresenceEntity, p)) {
            if (!s_presenceValid || p != s_presence) {
                LOG_I("HA obecnosc: %s -> %s", cfgHaPresenceEntity.c_str(), p ? "ON" : "OFF");
            }
            s_presence = p;
            s_presenceValid = true;
        }
    }

    if (now - s_lastPoll < HA_POLL_MS) return;
    s_lastPoll = now;

    float t, h;
    bool okT = fetch_state(cfgHaTempEntity, t);
    bool okH = fetch_state(cfgHaHumEntity, h);
    if (okT && okH) {
        if (t > -40.0f && t < 80.0f && h >= 0.0f && h <= 100.0f) {
            s_data.temp = t;
            s_data.hum  = h;
            s_data.valid = true;
            s_data.lastMs = now;
            static uint32_t lastLog = 0;
            if (now - lastLog > 60000) {
                lastLog = now;
                LOG_I("HA ext: %.1f C, %.1f %% RH", t, h);
            }
        }
    }
}

bool ha_presence() {
    if (!cfgPresenceLed) return true;      // obsluga wylaczona -> LED swieci
    if (!s_presenceValid) return true;     // brak odczytu -> nie gas LED blednie
    return s_presence;
}

HaData ha_get() { return s_data; }
