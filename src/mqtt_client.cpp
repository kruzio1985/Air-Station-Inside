#include "mqtt_client.h"
#include "app.h"
#include "config.h"
#include "nvs_store.h"
#include "syslog.h"
#include "sensors.h"
#include "led_strip.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <string.h>

static WiFiClient   wifiClient;
static PubSubClient mqtt(wifiClient);

static uint32_t s_lastPub = 0;
static bool     s_discoverySent = false;

static String state_topic(const char* id)   { return cfgMqttPrefix + "/sensor/" + id + "/state"; }
static String discovery_topic(const char* id){ return "homeassistant/sensor/" + cfgMqttPrefix + "_" + id + "/config"; }

static void on_message(char* topic, byte* payload, unsigned int len);

// ------------------------------------------------------------------------------
void mqtt_begin() {
    mqtt.setBufferSize(1024);
    mqtt.setServer(cfgMqttHost.c_str(), cfgMqttPort);
    mqtt.setKeepAlive(30);
    mqtt.setCallback(on_message);
    LOG_I("MQTT: broker %s:%d (prefix=%s)", cfgMqttHost.c_str(), cfgMqttPort, cfgMqttPrefix.c_str());
}

void mqtt_apply_settings() {
    mqtt.disconnect();
    mqtt.setServer(cfgMqttHost.c_str(), cfgMqttPort);
    s_discoverySent = false;
}

static void on_message(char* topic, byte* payload, unsigned int len) {
    payload[len] = 0;
    String t = String(topic);
    String p = String((char*)payload);

    // Komendy paska LED z HA (np. automatyzacja czujki Tuya)
    if (t == cfgMqttPrefix + "/led/mode") {
        if (p == "motion") { note_motion_from_mqtt(); LOG_I("MQTT: led/mode=motion"); }
        else {
            int m = led_mode_from_name(p.c_str());
            if (m >= 0) { led_set_mode(m, cfgLedSpeed, false); LOG_I("MQTT: led/mode=%s (tryb %d)", p.c_str(), m); }
            else LOG_I("MQTT: led/mode=%s", p.c_str());
        }
        return;
    }

    // Dane z wezla pojemnikow: <prefix>/box/<N>/temp|hum|meta  (N = 1..BOX_COUNT)
    String boxPrefix = cfgZ2mTopic + "/box/";
    if (t.startsWith(boxPrefix)) {
        String rest = t.substring(boxPrefix.length());
        int slash = rest.indexOf('/');
        if (slash > 0) {
            int n = rest.substring(0, slash).toInt();
            String kind = rest.substring(slash + 1);
            if (n >= 1 && n <= BOX_COUNT) {
                int i = n - 1;
                if (kind == "temp") {
                    boxes[i].temp = p.toFloat();
                    boxes[i].online = true;
                    boxes[i].lastMs = millis();
                } else if (kind == "hum") {
                    boxes[i].hum = p.toFloat();
                    boxes[i].online = true;
                    boxes[i].lastMs = millis();
                } else if (kind == "meta" && cfgBoxMetaFromNode) {
                    JsonDocument d;
                    if (!deserializeJson(d, p)) {
                        if (d["manufacturer"].is<const char*>()) strncpy(boxMeta[i].brand, d["manufacturer"].as<const char*>(), sizeof(boxMeta[i].brand) - 1);
                        if (d["type"].is<const char*>())         strncpy(boxMeta[i].type,  d["type"].as<const char*>(),  sizeof(boxMeta[i].type)  - 1);
                        if (d["color"].is<const char*>())        strncpy(boxMeta[i].color, d["color"].as<const char*>(), sizeof(boxMeta[i].color) - 1);
                    }
                }
            }
        }
        return;
    }

    // Stary format: pojedynczy JSON z kluczami boxN_temperature/boxN_humidity
    if (t == cfgZ2mTopic) {
        JsonDocument doc;
        if (deserializeJson(doc, p)) return;
        for (int i = 0; i < BOX_COUNT; i++) {
            char kt[24], kh[24];
            snprintf(kt, sizeof(kt), "box%d_temperature", i + 1);
            snprintf(kh, sizeof(kh), "box%d_humidity",    i + 1);
            if (doc[kt].is<float>() || doc[kt].is<int>()) { boxes[i].temp = doc[kt].as<float>(); boxes[i].online = true; boxes[i].lastMs = millis(); }
            if (doc[kh].is<float>() || doc[kh].is<int>()) { boxes[i].hum  = doc[kh].as<float>(); boxes[i].online = true; boxes[i].lastMs = millis(); }
        }
        return;
    }
}

// ------------------------------------------------------------------------------
void mqtt_loop() {
    if (cfgMqttHost.length() == 0) return;   // MQTT wylaczone

    if (!mqtt.connected()) {
        static uint32_t lastTry = 0;
        if (millis() - lastTry < 5000) return;
        lastTry = millis();
        String id = cfgHostname + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
        if (mqtt.connect(id.c_str(), cfgMqttUser.c_str(), cfgMqttPass.c_str(),
                         (cfgMqttPrefix + "/status").c_str(), 0, true, "offline")) {
            LOG_I("MQTT: polaczono z %s", cfgMqttHost.c_str());
            mqtt.publish((cfgMqttPrefix + "/status").c_str(), "online", true);
            mqtt.subscribe((cfgMqttPrefix + "/led/mode").c_str());
            mqtt.subscribe((cfgZ2mTopic + "/box/#").c_str());
            mqtt.subscribe(cfgZ2mTopic.c_str());
            s_discoverySent = false;
        } else {
            LOG_E("MQTT: brak polaczenia (kod %d)", mqtt.state());
        }
        return;
    }

    mqtt.loop();

    uint32_t now = millis();
    if (now - s_lastPub >= MQTT_INTERVAL_MS) {
        s_lastPub = now;

        // discovery (raz po polaczeniu)
        if (!s_discoverySent) {
            for (int i = 0; i < channelCount; i++) {
                Channel& c = channels[i];
                JsonDocument d;
                d["name"] = String(DEVICE_NAME) + " " + c.name;
                d["state_topic"] = state_topic(c.id);
                d["unique_id"] = String(cfgHostname) + "_" + c.id;
                if (c.unit[0]) d["unit_of_measurement"] = c.unit;
                d["device"]["name"] = DEVICE_NAME;
                d["device"]["identifiers"] = cfgHostname;
                d["device"]["model"] = "ESP32-S3 N16R8";
                d["device"]["manufacturer"] = "kruzio1985";
                String out; serializeJson(d, out);
                mqtt.publish(discovery_topic(c.id).c_str(), out.c_str(), true);
            }
            s_discoverySent = true;
            LOG_I("MQTT: discovery opublikowane (%d czujnikow)", channelCount);
        }

        // stan kanalow
        for (int i = 0; i < channelCount; i++) {
            Channel& c = channels[i];
            if (!c.valid) continue;
            char buf[32];
            snprintf(buf, sizeof(buf), "%.2f", c.value);
            mqtt.publish(state_topic(c.id).c_str(), buf);
        }

        // stan pojemnikow (tam, gdzie sa dane)
        for (int i = 0; i < BOX_COUNT; i++) {
            if (!boxes[i].online) continue;
            char t[24], v[16];
            snprintf(t, sizeof(t), "%s/box/%d/temp", cfgMqttPrefix.c_str(), i + 1);
            snprintf(v, sizeof(v), "%.1f", boxes[i].temp);
            mqtt.publish(t, v);
            snprintf(t, sizeof(t), "%s/box/%d/hum", cfgMqttPrefix.c_str(), i + 1);
            snprintf(v, sizeof(v), "%.1f", boxes[i].hum);
            mqtt.publish(t, v);
        }
    }

    // --- Zbiorczy JSON jakosci powietrza dla stacji pogodowej (BEZ temp/wilg.) ---
    // Publikowany co 30 s jako retained na cfgAirTopic. Do odczytu przez
    // stacje pogodowa (osobny czat) - pola ponizej.
    static uint32_t s_lastAir = 0;
    if (cfgAirEnabled && cfgAirTopic.length() && now - s_lastAir >= 30000) {
        s_lastAir = now;
        JsonDocument d;
        d["ts"] = (uint32_t)time(nullptr);
        const char* ids[]  = {"pm1", "pm25", "pm10", "aqi", "co2", "tvoc", "ch2o", "mics_co", "mq7_co", "tvoclvl"};
        const char* keys[] = {"pm1", "pm25", "pm10", "aqi", "co2", "tvoc", "ch2o", "mics_co", "mq7_co", "tvoc_level"};
        for (int i = 0; i < 10; i++) {
            Channel* ch = channel_find(ids[i]);
            if (ch && ch->valid) d[keys[i]] = ch->value;
        }
        String out;
        serializeJson(d, out);
        mqtt.publish(cfgAirTopic.c_str(), out.c_str(), true);
    }
}

bool mqtt_connected() { return mqtt.connected(); }
