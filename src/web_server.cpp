#include "web_server.h"
#include "app.h"
#include "pins.h"
#include "config.h"
#include "nvs_store.h"
#include "syslog.h"
#include "sensors.h"
#include "mqtt_client.h"
#include "sd_card.h"
#include "wifi_mgr.h"
#include "led_strip.h"
#include "drv_ads1115.h"
#include "aqi.h"
#include "agg.h"
#include <LittleFS.h>
#include <SD.h>
#include <WiFi.h>
#include <Update.h>
#include <esp_system.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <ESP.h>
#include <time.h>
#include <math.h>

static AsyncWebServer server(80);

// ------------------------------------------------------------------------------
//  Odbiór treści żądań POST (tak jak w StacjaPogody):
//  przy "application/json" body trafia TYLKO do odbiornika server.on(...).
// ------------------------------------------------------------------------------
static String s_body;
static AsyncWebServerRequest* s_bodyReq = nullptr;
static const size_t MAX_BODY = 24576;

static void bodySink(AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t index, size_t total) {
    (void)total;
    if (index == 0) { s_body = ""; s_bodyReq = r; }
    if (r != s_bodyReq) return;
    if (s_body.length() + len > MAX_BODY) return;
    s_body.concat((const char*)data, len);
}

static void onPost(const char* uri, ArRequestHandlerFunction fn) {
    server.on(uri, HTTP_POST, fn, nullptr, bodySink);
}

static String bodyOf(AsyncWebServerRequest* r) {
    if (r == s_bodyReq && s_body.length()) return s_body;
    String b = r->arg("plain");
    if (b.length() == 0 && r->params()) {
        for (size_t i = 0; i < r->params(); i++) {
            const AsyncWebParameter* p = r->getParam(i);
            if (p->isPost()) {
                if (b.length()) b += "&";
                b += p->name() + "=" + p->value();
            }
        }
    }
    return b;
}

static String jsonError(const char* msg) {
    JsonDocument d;
    d["error"] = msg;
    String s;
    serializeJson(d, s);
    return s;
}

static const char* reset_reason_str() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON:   return "zasilanie (POWERON)";
        case ESP_RST_EXT:       return "zewnetrzny (RESET/EN)";
        case ESP_RST_SW:        return "programowy (ESP.restart/OTA)";
        case ESP_RST_PANIC:     return "PANIC (wyjatek / blad krytyczny)";
        case ESP_RST_INT_WDT:   return "interrupt watchdog";
        case ESP_RST_TASK_WDT:  return "task watchdog (zawieszenie petli)";
        case ESP_RST_WDT:       return "watchdog";
        case ESP_RST_DEEPSLEEP: return "gleboki sen";
        case ESP_RST_BROWNOUT:  return "BROWNOUT (spadek napiecia)";
        case ESP_RST_SDIO:      return "SDIO";
        default:                return "nieznany";
    }
}

static bool parseHexColor(const char* s, uint32_t& out) {
    if (!s) return false;
    while (*s == ' ' || *s == '#') s++;
    uint32_t v = 0; int n = 0;
    for (; *s; s++) {
        int d;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (*s >= 'a' && *s <= 'f') d = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') d = *s - 'A' + 10;
        else return false;
        v = (v << 4) | (uint32_t)d;
        if (++n > 6) return false;
    }
    if (n != 6) return false;
    out = v;
    return true;
}

// ------------------------------------------------------------------------------
static String apiStatusJson() {
    JsonDocument d;

    d["fw"] = FW_NAME;
    d["version"] = FW_VERSION;
    d["uptime_s"] = (uint32_t)(millis() / 1000);
    d["free_heap"] = (uint32_t)ESP.getFreeHeap();
    d["free_psram"] = (uint32_t)ESP.getFreePsram();

    struct tm t;
    if (getLocalTime(&t, 100)) {
        char ts[32];
        snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d",
                 t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
        d["time"] = ts;
    }

    d["wifi_mode"] = wifi_mode_label();
    d["mqtt"] = mqtt_connected();
    d["sd"] = sd_ready();
    d["pir_raw"] = pir_raw_level();
    d["mics_heater"] = ads_heater_on() ? 1 : 0;
    d["led_mode"] = led_current_mode();
    d["led_speed"] = led_current_speed();
    d["led_mode_name"] = led_mode_name(led_current_mode());
    d["led_order"] = cfgLedOrder;

    JsonArray ch = d["channels"].to<JsonArray>();
    for (int i = 0; i < channelCount; i++) {
        JsonObject o = ch.add<JsonObject>();
        o["id"] = channels[i].id;
        o["name"] = channels[i].name;
        o["unit"] = channels[i].unit;
        o["value"] = channels[i].valid ? channels[i].value : NAN;
        o["valid"] = channels[i].valid;
        if (channels[i].id[0] == 'a' && strcmp(channels[i].id, "aqi") == 0) {
            o["label"] = aqi_label((int)channels[i].value);
            o["color"] = String(aqi_color((int)channels[i].value), HEX);
        }
    }

    JsonArray bx = d["boxes"].to<JsonArray>();
    for (int i = 0; i < BOX_COUNT; i++) {
        JsonObject o = bx.add<JsonObject>();
        o["n"] = i + 1;
        o["online"] = boxes[i].online;
        o["temp"] = boxes[i].online ? boxes[i].temp : NAN;
        o["hum"] = boxes[i].online ? boxes[i].hum : NAN;
        o["state"] = !boxes[i].online ? "brak" :
                     (boxes[i].hum > cfgBoxRhAlarm ? "alarm" :
                      boxes[i].hum > cfgBoxRhWarn  ? "warn" : "ok");
        o["name"] = boxMeta[i].name;
        o["type"] = boxMeta[i].type;
        o["color"] = boxMeta[i].color;
        o["brand"] = boxMeta[i].brand;
        o["target_rh"] = boxMeta[i].targetRh;
    }

    String out;
    serializeJson(d, out);
    return out;
}

// ------------------------------------------------------------------------------
void web_begin() {
    // --- Status (podglad na pulpicie) ---
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, "application/json", apiStatusJson());
    });

    // --- Historia (rekordy minutowe) do wykresow ---
    server.on("/api/history", HTTP_GET, [](AsyncWebServerRequest* r) {
        int hours = r->hasArg("hours") ? r->arg("hours").toInt() : 12;
        if (hours <= 0) hours = 12;
        if (hours > 24) hours = 24;
        uint32_t maxPts = (uint32_t)hours * 60;
        uint32_t total  = agg_count();
        uint32_t start  = (total > maxPts) ? (total - maxPts) : 0;

        JsonDocument d;
        JsonArray arr = d["points"].to<JsonArray>();
        for (uint32_t i = start; i < total; i++) {
            MinuteRec rec;
            if (!agg_get(i, rec)) continue;
            JsonObject o = arr.add<JsonObject>();
            o["ts"]   = rec.ts;
            o["pm1"]  = rec.pm1;
            o["pm25"] = rec.pm25;
            o["pm10"] = rec.pm10;
            o["co2"]  = rec.co2;
            o["tvoc"] = rec.tvoc;
            o["ch2o"] = rec.ch2o;
            o["temp"] = rec.temp;
            o["hum"]  = rec.hum;
        }
        String out;
        serializeJson(d, out);
        r->send(200, "application/json", out);
    });

    // --- Konfiguracja ---
    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, "application/json", nvs_export_json());
    });

    onPost("/api/config", [](AsyncWebServerRequest* r) {
        String b = bodyOf(r);
        if (b.length() == 0) {
            r->send(400, "application/json", jsonError("Puste żądanie - brak danych"));
            return;
        }
        if (nvs_import_json(b)) {
            mqtt_apply_settings();
            led_apply_settings();
            r->send(200, "application/json", "{\"ok\":true}");
        } else {
            r->send(400, "application/json", jsonError("Błędny JSON"));
        }
    });

    // --- Kopia zapasowa ---
    server.on("/api/backup", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, "application/json", nvs_export_json());
    });
    onPost("/api/backup/restore", [](AsyncWebServerRequest* r) {
        String b = bodyOf(r);
        if (nvs_import_json(b)) { mqtt_apply_settings(); led_apply_settings(); r->send(200, "application/json", "{\"ok\":true}"); }
        else r->send(400, "application/json", jsonError("Błędny JSON"));
    });

    // --- Kalibracja MiCS5524: zapisz obecne napiecie jako R0 (czyste powietrze) ---
    onPost("/api/calibrate", [](AsyncWebServerRequest* r) {
        MicsData m = ads_mics_get();
        if (!m.valid) { r->send(400, "application/json", jsonError("Brak odczytu z ADS1115")); return; }
        cfgMicsR0 = m.vout;
        nvs_save();
        char out[64];
        snprintf(out, sizeof(out), "{\"ok\":true,\"r0\":%.3f}", m.vout);
        r->send(200, "application/json", out);
        LOG_I("Kalibracja: MiCS5524 R0 = %.3f V", m.vout);
    });

    // --- Kalibracja MQ-7: zapisz obecne napiecie jako R0 (czyste powietrze) ---
    onPost("/api/calibrate_mq7", [](AsyncWebServerRequest* r) {
        Mq7Data mq = ads_mq7_get();
        if (!mq.valid) { r->send(400, "application/json", jsonError("Brak odczytu z ADS1115 (AIN2)")); return; }
        cfgMq7R0 = mq.vout;
        nvs_save();
        char out[64];
        snprintf(out, sizeof(out), "{\"ok\":true,\"r0\":%.3f}", mq.vout);
        r->send(200, "application/json", out);
        LOG_I("Kalibracja: MQ-7 R0 = %.3f V", mq.vout);
    });

    // --- Sterowanie grzalka MiCS5524 (EN, GPIO) ---
    onPost("/api/heater", [](AsyncWebServerRequest* r) {
        JsonDocument d;
        bool on = true;
        if (deserializeJson(d, bodyOf(r))) {
            // dopuszczamy tez query ?on=0/1
            if (r->hasArg("on")) on = (r->arg("on") == "1" || r->arg("on") == "true");
        } else if (d["on"].is<bool>()) {
            on = d["on"].as<bool>();
        }
        ads_set_heater(on);
        r->send(200, "application/json", String("{\"ok\":true,\"heater\":") + (on ? "1" : "0") + "}");
    });

    // --- Test paska / matrycy LED + wybor efektu ---
    onPost("/api/led", [](AsyncWebServerRequest* r) {
        JsonDocument d;
        if (deserializeJson(d, bodyOf(r))) { r->send(400, "application/json", jsonError("Błędny JSON")); return; }
        // Wybor efektu: {"effect":"rainbow","speed":5,"save":true,"order":0,"brightness":127,"layout":0,"rotate":0,"flip_y":false,"font":0,"value_color":0,"text":"..."}
        if (d["effect"].is<const char*>()) {
            int m = led_mode_from_name(d["effect"].as<const char*>());
            int sp = d["speed"].is<int>() ? d["speed"].as<int>() : led_current_speed();
            bool save = d["save"].is<bool>() ? d["save"].as<bool>() : false;
            if (m < 0) { r->send(400, "application/json", jsonError("Nieznany efekt")); return; }
            if (d["layout"].is<int>())  cfgLedLayout = d["layout"].as<int>() ? 1 : 0;
            if (d["rotate"].is<int>())  cfgLedRotate = ((d["rotate"].as<int>() % 4) + 4) % 4;
            if (d["flip_y"].is<bool>()) cfgLedFlipY = d["flip_y"].as<bool>();
            if (d["font"].is<int>())    cfgLedFont = d["font"].as<int>() ? 1 : 0;
            if (d["value_color"].is<int>()) cfgLedValueColor = d["value_color"].as<int>();
            if (d["text"].is<const char*>()) cfgLedText = d["text"].as<const char*>();
            if (d["order"].is<int>()) led_set_order(d["order"].as<int>(), save);
            if (d["brightness"].is<int>()) led_set_brightness(d["brightness"].as<int>(), save);
            led_set_mode(m, sp, save);
            r->send(200, "application/json", "{\"ok\":true}");
            return;
        }
        // Ustawienia dodatkowe (na zywo): {"font":1,"value_color":3,"text":"PM 2.5","save":true}
        if (d["font"].is<int>() || d["value_color"].is<int>() || d["text"].is<const char*>()) {
            if (d["font"].is<int>())    cfgLedFont = d["font"].as<int>() ? 1 : 0;
            if (d["value_color"].is<int>()) cfgLedValueColor = d["value_color"].as<int>();
            if (d["text"].is<const char*>()) cfgLedText = d["text"].as<const char*>();
            bool save = d["save"].is<bool>() ? d["save"].as<bool>() : false;
            if (save) nvs_save();
            r->send(200, "application/json", "{\"ok\":true}");
            return;
        }
        // Orientacja matrycy (na zywo): {"layout":1,"rotate":2,"flip_y":false,"save":true}
        if (d["layout"].is<int>() || d["rotate"].is<int>() || d["flip_y"].is<bool>()) {
            if (d["layout"].is<int>())  cfgLedLayout = d["layout"].as<int>() ? 1 : 0;
            if (d["rotate"].is<int>())  cfgLedRotate = ((d["rotate"].as<int>() % 4) + 4) % 4;
            if (d["flip_y"].is<bool>()) cfgLedFlipY = d["flip_y"].as<bool>();
            bool save = d["save"].is<bool>() ? d["save"].as<bool>() : false;
            if (save) nvs_save();
            r->send(200, "application/json", "{\"ok\":true}");
            return;
        }
        // Sama kolejnosci kolorow: {"order":3,"save":true}
        if (d["order"].is<int>()) {
            bool save = d["save"].is<bool>() ? d["save"].as<bool>() : false;
            led_set_order(d["order"].as<int>(), save);
            r->send(200, "application/json", "{\"ok\":true}");
            return;
        }
        // Sama jasnosc: {"brightness":127,"save":true}
        if (d["brightness"].is<int>()) {
            bool save = d["save"].is<bool>() ? d["save"].as<bool>() : false;
            led_set_brightness(d["brightness"].as<int>(), save);
            r->send(200, "application/json", "{\"ok\":true}");
            return;
        }
        // Diagnostyka: {"dots":1,"ms":25000} - 4 diody (czerwona/zielona/niebieska/biala)
        if (d["dots"].is<int>()) {
            uint16_t ms = d["ms"].is<int>() ? (uint16_t)d["ms"].as<int>() : 25000;
            led_test_dots(ms);
            r->send(200, "application/json", "{\"ok\":true}");
            return;
        }
        // Test koloru: {"color":"00FF00","ms":3000}
        uint32_t color = 0x00FF00;
        uint16_t ms = 3000;
        if (d["color"].is<const char*>()) parseHexColor(d["color"].as<const char*>(), color);
        if (d["ms"].is<int>()) ms = d["ms"].as<int>();
        led_test(color, ms);
        r->send(200, "application/json", "{\"ok\":true}");
    });

    // --- Logi ---
    server.on("/api/syslog", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, "text/plain", syslog_dump(false));
    });
    server.on("/api/syslog", HTTP_DELETE, [](AsyncWebServerRequest* r) {
        syslog_dump(true);
        r->send(200, "application/json", "{\"ok\":true}");
    });

    // --- Karta SD: lista plikow CSV + pobieranie + usuwanie ---
    server.on("/api/sd/list", HTTP_GET, [](AsyncWebServerRequest* r) {
        if (!sd_ready()) { r->send(400, "application/json", jsonError("SD nie podlaczona")); return; }
        JsonDocument d;
        JsonArray arr = d["files"].to<JsonArray>();
        File dir = SD.open(SD_LOG_DIR);
        if (dir && dir.isDirectory()) {
            File f = dir.openNextFile();
            while (f) {
                if (!f.isDirectory()) {
                    JsonObject o = arr.add<JsonObject>();
                    o["name"] = String(f.name());
                    o["size"] = (uint32_t)f.size();
                }
                f = dir.openNextFile();
            }
        }
        String out;
        serializeJson(d, out);
        r->send(200, "application/json", out);
    });

    server.on("/api/sd/file", HTTP_GET, [](AsyncWebServerRequest* r) {
        if (!sd_ready()) { r->send(400, "text/plain", "SD nie podlaczona"); return; }
        String name = r->arg("name");
        if (name.length() == 0 || name.indexOf('/') >= 0 || name.indexOf("..") >= 0) {
            r->send(400, "text/plain", "Bledna nazwa pliku");
            return;
        }
        String path = String(SD_LOG_DIR) + "/" + name;
        if (!SD.exists(path)) { r->send(404, "text/plain", "Brak pliku"); return; }
        r->send(SD, path, "text/csv", true);   // true = pobierz (attachment)
    });

    server.on("/api/sd/delete", HTTP_DELETE, [](AsyncWebServerRequest* r) {
        if (!sd_ready()) { r->send(400, "application/json", jsonError("SD nie podlaczona")); return; }
        String name = r->arg("name");
        if (name.length() == 0 || name.indexOf('/') >= 0 || name.indexOf("..") >= 0) {
            r->send(400, "application/json", jsonError("Bledna nazwa pliku"));
            return;
        }
        String path = String(SD_LOG_DIR) + "/" + name;
        if (SD.remove(path)) r->send(200, "application/json", "{\"ok\":true}");
        else r->send(500, "application/json", jsonError("Nie udalo sie usunac pliku"));
    });

    // --- Diagnostyka: firmware, reset reason, pamiec, status ---
    server.on("/api/diag", HTTP_GET, [](AsyncWebServerRequest* r) {
        JsonDocument d;
        d["fw"] = FW_NAME;
        d["version"] = FW_VERSION;
        d["build"] = String(__DATE__) + " " + String(__TIME__);
        d["uptime_s"] = (uint32_t)(millis() / 1000);
        d["reset_reason"] = reset_reason_str();
        d["free_heap"] = (uint32_t)ESP.getFreeHeap();
        d["free_psram"] = (uint32_t)ESP.getFreePsram();
        d["wifi_rssi"] = (int)WiFi.RSSI();
        d["wifi_ip"] = WiFi.localIP().toString();
        d["wifi_mac"] = WiFi.macAddress();
        d["mqtt"] = mqtt_connected();
        d["sd"] = sd_ready();
        d["littlefs_total"] = (uint32_t)LittleFS.totalBytes();
        d["littlefs_used"] = (uint32_t)LittleFS.usedBytes();
        int validCount = 0;
        for (int i = 0; i < channelCount; i++) if (channels[i].valid) validCount++;
        d["channels_valid"] = validCount;
        d["channels_total"] = channelCount;
        String out;
        serializeJson(d, out);
        r->send(200, "application/json", out);
    });

    // --- OTA: wgranie firmware .bin z przegladarki ---
    server.on("/api/ota", HTTP_POST,
        [](AsyncWebServerRequest* request) {
            if (Update.hasError()) {
                request->send(500, "application/json", jsonError("Blad aktualizacji"));
            } else {
                request->send(200, "application/json", "{\"ok\":true}");
                delay(300);
                ESP.restart();
            }
        },
        [](AsyncWebServerRequest* request, const String& filename, size_t index,
           uint8_t* data, size_t len, bool final) {
            if (!index) {
                LOG_I("OTA www: start %s (%u B)", filename.c_str(),
                      (unsigned)request->contentLength());
                if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
                    Update.printError(Serial);
                }
            }
            if (Update.write(data, len) != len) {
                Update.abort();
            }
            if (final) {
                if (!Update.end(true)) {
                    LOG_E("OTA www: blad koncowy");
                } else {
                    LOG_I("OTA www: zakonczono OK, restart");
                }
            }
        });

    // --- Wgrywanie pojedynczego pliku strony do LittleFS (bez USB) ---
    server.on("/api/file", HTTP_POST,
        [](AsyncWebServerRequest* request) {
            request->send(200, "application/json", "{\"ok\":true}");
        },
        [](AsyncWebServerRequest* request, const String& filename, size_t index,
           uint8_t* data, size_t len, bool final) {
            static File upf;
            if (!index) {
                if (upf) upf.close();
                if (filename.length() == 0 || filename.indexOf('/') >= 0 || filename.indexOf("..") >= 0) {
                    LOG_E("FS upload: bledna nazwa pliku");
                    return;
                }
                String path = "/" + filename;
                upf = LittleFS.open(path, FILE_WRITE);
                if (!upf) LOG_E("FS upload: blad otwarcia %s", path.c_str());
            }
            if (upf) upf.write(data, len);
            if (final && upf) {
                upf.close();
                LOG_I("FS upload: zapisano %s (%u B)", filename.c_str(), (unsigned)(index + len));
            }
        });

    // --- Restart / reset fabryczny ---
    onPost("/api/reboot", [](AsyncWebServerRequest* r) {
        r->send(200, "application/json", "{\"ok\":true}");
        delay(200);
        ESP.restart();
    });
    onPost("/api/factory", [](AsyncWebServerRequest* r) {
        r->send(200, "application/json", "{\"ok\":true}");
        delay(200);
        nvs_factory_reset();
        ESP.restart();
    });

    // --- Pliki statyczne (SPA) ---
    server.serveStatic("/", LittleFS, "/", "no-cache, must-revalidate").setDefaultFile("index.html");

    server.onNotFound([](AsyncWebServerRequest* r) {
        if (r->method() == HTTP_GET && r->url().indexOf('.') < 0) r->redirect("/");
        else r->send(404, "text/plain", "404");
    });

    server.begin();
    LOG_I("Serwer www wystartowal (port 80)");
}
