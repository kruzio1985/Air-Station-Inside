// =============================================================================
//  FilamentMonitor - monitor powietrza i wilgotnosci filamentu przy drukarce 3D
//  Master: ESP32-S3 WROOM-1 N16R8 (osobna plytka)
//  Copyright (c) 2026 kruzio1985
// =============================================================================
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "app.h"
#include "syslog.h"
#include "nvs_store.h"
#include "sensors.h"
#include "led_strip.h"
#include "wifi_mgr.h"
#include "mqtt_client.h"
#include "sd_card.h"
#include "web_server.h"

void setup() {
    Serial.begin(115200);
    delay(300);

    syslog_init();
    LOG_I("==============================================================");
    LOG_I("%s v%s - monitor powietrza i filamentu", FW_NAME, FW_VERSION);
    LOG_I("Plytka: ESP32-S3 N16R8 (PSRAM %s)", ESP.getPsramSize() ? "OK" : "BRAK");

    nvs_begin();
    sensors_begin();
    led_begin();

    if (!LittleFS.begin()) {
        LOG_E("LittleFS: nie udalo sie zamontowac - strona www niedostepna");
    } else {
        LOG_I("LittleFS: zamontowany (%.1f kB)", LittleFS.totalBytes() / 1024.0);
    }

    wifi_begin();
    mqtt_begin();
    sd_begin();
    web_begin();

    // --- Watchdog sprzetowy: auto-restart, jesli petla sie zawiesi na >30 s ---
    // ESP-IDF 4.4 (rdzen 2.0.17): esp_task_wdt_init(timeout_s, panic).
    // panic=false -> czysty reset zamiast paniki.
    if (esp_task_wdt_init(30, false) == ESP_OK && esp_task_wdt_add(NULL) == ESP_OK) {
        LOG_I("Watchdog: 30 s (auto-restart przy zawieszeniu)");
    } else {
        LOG_W("Watchdog: nie udalo sie wlaczyc");
    }

    // --- OTA przez Wi-Fi (pio run -e esp32s3-ota -t upload) ---------------------
    ArduinoOTA.setHostname(FW_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA
        .onStart([]() { LOG_I("OTA: rozpoczecie aktualizacji"); })
        .onEnd([]()   { LOG_I("OTA: zakonczono"); })
        .onProgress([](unsigned int p, unsigned int total) {
            if (total == 0) return;
            static uint32_t lastPct = 0;
            unsigned pct = (unsigned)((p * 100ULL) / total);
            if (millis() - lastPct > 2000) { lastPct = millis(); LOG_I("OTA: %u%%", pct); }
        })
        .onError([](ota_error_t e) { LOG_E("OTA: blad %d", e); });
    ArduinoOTA.begin();
    LOG_I("OTA: gotowe (host=%s, port 3232)", FW_HOSTNAME);

    LOG_I("Start zakonczony - czekam na pomiary");
}

void loop() {
    ArduinoOTA.handle();
    esp_task_wdt_reset();
    wifi_loop();
    sensors_loop();
    mqtt_loop();
    sd_loop();
    led_loop();
    delay(20);
}
