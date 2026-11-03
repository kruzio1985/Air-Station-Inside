#include "nvs_store.h"
#include "config.h"
#include "app.h"
#include "syslog.h"
#include <Preferences.h>
#include <ArduinoJson.h>

// --- Wartosci globalne ---------------------------------------------------------
String cfgWifiSsid   = "";
String cfgWifiPass   = "";
String cfgHostname   = FW_HOSTNAME;
bool   cfgWifiStatic = true;
String cfgWifiIp     = "192.168.1.141";
String cfgWifiMask   = "255.255.255.0";
String cfgWifiGw     = "192.168.1.1";
String cfgWifiDns    = "192.168.1.1";

String cfgMqttHost   = "192.168.1.14";   // broker HA (Mosquitto) - jak panel Guiton 4
int    cfgMqttPort   = MQTT_PORT_DEFAULT;
String cfgMqttUser   = "";
String cfgMqttPass   = "";
String cfgMqttPrefix = MQTT_PREFIX_DEFAULT;
String cfgZ2mTopic   = Z2M_TOPIC_DEFAULT;
bool   cfgBoxMetaFromNode = true;

String cfgHaUrl        = "http://192.168.1.14:8123";
String cfgHaToken      = "";
String cfgHaTempEntity = "sensor.temperature_humidity_sensor_5_temperatura";
String cfgHaHumEntity  = "sensor.temperature_humidity_sensor_5_wilgotnosc";

bool   cfgAirEnabled  = true;
String cfgAirTopic    = "stacja_powietrza/air";

int    cfgLedCount       = LED_COUNT_DEFAULT;
int    cfgLedBrightness  = LED_BRIGHT_DEFAULT;
int    cfgLedMaxMa       = LED_MAX_MA_DEFAULT;

bool   cfgLedMatrix    = true;   // matryca 8x8 (64 diody)
int    cfgLedMode      = 0;      // 0=powietrze (domyslne)
int    cfgLedSpeed     = 5;      // 1..10
int    cfgLedLayout    = 0;      // serpentine (zygzak)
int    cfgLedRotate    = 0;      // bez obrotu
bool   cfgLedFlipY     = false;  // bez odbicia
int    cfgLedOrder     = 0;      // GRB
int    cfgLedFont      = 0;      // mala czcionka
int    cfgLedValueColor = 0;     // auto (kolor jakosci powietrza)
bool   cfgLedAlwaysOn  = true;   // matryca swieci bez wzgledu na ruch
String cfgLedText      = "";     // pusty = zegar HH:MM w trybie tekst

float  cfgMicsR0    = 0.0f;   // 0 = nie skalibrowane (pomiar surowy)
float  cfgMq7R0     = 0.0f;   // 0 = nie skalibrowane (MQ-7, czyste powietrze)
float  cfgPmOffset  = 0.0f;

bool   cfgPmRhCorr  = true;
float  cfgPmRhThresh = PM_RH_THRESH_DEFAULT;
float  cfgPmRhCoef   = PM_RH_COEF_DEFAULT;

float  cfgBoxRhWarn  = BOX_RH_WARN;
float  cfgBoxRhAlarm = BOX_RH_ALARM;

bool  cfgPirEnabled    = true;
bool  cfgPirInverted   = false;   // false = aktywny HIGH (SR505), true = aktywny NISKI
int   cfgPirHoldMs     = 60000;   // ruch aktywny przez 60 s po zdarzeniu
int   cfgPirDebounceMs = 100;     // ignoruj impulsy krotsze niz 100 ms

static Preferences prefs;

// --- Pojemniki: domyslne metadane + (de)serializacja JSON -----------------------
static void boxes_defaults() {
    for (int i = 0; i < BOX_COUNT; i++) {
        snprintf(boxMeta[i].name, sizeof(boxMeta[i].name), "Pojemnik %d", i + 1);
        strncpy(boxMeta[i].type, "PLA", sizeof(boxMeta[i].type) - 1);
        boxMeta[i].color[0] = 0;
        boxMeta[i].brand[0] = 0;
        boxMeta[i].targetRh = 30.0f;
    }
}

static String boxes_to_json() {
    JsonDocument d;
    JsonArray arr = d.to<JsonArray>();
    for (int i = 0; i < BOX_COUNT; i++) {
        JsonObject o = arr.add<JsonObject>();
        o["name"] = boxMeta[i].name;
        o["type"] = boxMeta[i].type;
        o["color"] = boxMeta[i].color;
        o["brand"] = boxMeta[i].brand;
        o["target_rh"] = boxMeta[i].targetRh;
    }
    String s;
    serializeJson(d, s);
    return s;
}

static void boxes_from_json(const char* s) {
    JsonDocument d;
    if (deserializeJson(d, s)) return;
    JsonArray arr = d.as<JsonArray>();
    for (int i = 0; i < BOX_COUNT && i < (int)arr.size(); i++) {
        JsonObject o = arr[i];
        if (o["name"].is<const char*>())  strncpy(boxMeta[i].name,  o["name"].as<const char*>(),  sizeof(boxMeta[i].name)  - 1);
        if (o["type"].is<const char*>())  strncpy(boxMeta[i].type,  o["type"].as<const char*>(),  sizeof(boxMeta[i].type)  - 1);
        if (o["color"].is<const char*>()) strncpy(boxMeta[i].color, o["color"].as<const char*>(), sizeof(boxMeta[i].color) - 1);
        if (o["brand"].is<const char*>()) strncpy(boxMeta[i].brand, o["brand"].as<const char*>(), sizeof(boxMeta[i].brand) - 1);
        if (o["target_rh"].is<float>())   boxMeta[i].targetRh = o["target_rh"].as<float>();
    }
}

// ------------------------------------------------------------------------------
void nvs_begin() {
    if (!prefs.begin("filament", false)) {
        Serial.println("E: NVS open failed");
        return;
    }
    // Wczytaj (domyslne zostaja, gdy klucza nie ma)
    cfgWifiSsid      = prefs.getString("w_ssid", cfgWifiSsid);
    cfgWifiPass      = prefs.getString("w_pass", cfgWifiPass);
    cfgHostname      = prefs.getString("host",   cfgHostname);
    cfgWifiStatic    = prefs.getBool("w_static", cfgWifiStatic);
    cfgWifiIp        = prefs.getString("w_ip",   cfgWifiIp);
    cfgWifiMask      = prefs.getString("w_mask", cfgWifiMask);
    cfgWifiGw        = prefs.getString("w_gw",   cfgWifiGw);
    cfgWifiDns       = prefs.getString("w_dns",  cfgWifiDns);

    cfgMqttHost      = prefs.getString("m_host",  cfgMqttHost);
    cfgMqttPort      = prefs.getInt("m_port",     cfgMqttPort);
    cfgMqttUser      = prefs.getString("m_user",  cfgMqttUser);
    cfgMqttPass      = prefs.getString("m_pass",  cfgMqttPass);
    cfgMqttPrefix    = prefs.getString("m_pref",  cfgMqttPrefix);
    cfgZ2mTopic      = prefs.getString("z2m",     cfgZ2mTopic);
    cfgBoxMetaFromNode = prefs.getBool("box_meta", cfgBoxMetaFromNode);

    cfgHaUrl         = prefs.getString("ha_url",   cfgHaUrl);
    cfgHaToken       = prefs.getString("ha_token", cfgHaToken);
    cfgHaTempEntity  = prefs.getString("ha_temp",  cfgHaTempEntity);
    cfgHaHumEntity   = prefs.getString("ha_hum",   cfgHaHumEntity);

    cfgAirEnabled    = prefs.getBool("air_en",    cfgAirEnabled);
    cfgAirTopic      = prefs.getString("air_topic", cfgAirTopic);

    cfgLedCount      = prefs.getInt("led_cnt", cfgLedCount);
    cfgLedBrightness = prefs.getInt("led_bri", cfgLedBrightness);
    cfgLedMaxMa      = prefs.getInt("led_ma",  cfgLedMaxMa);

    cfgLedMatrix     = prefs.getBool("led_mat",  cfgLedMatrix);
    cfgLedMode       = prefs.getInt("led_mode",  cfgLedMode);
    cfgLedSpeed      = prefs.getInt("led_spd",   cfgLedSpeed);
    cfgLedLayout     = prefs.getInt("led_lay",   cfgLedLayout);
    cfgLedRotate     = prefs.getInt("led_rot",   cfgLedRotate);
    cfgLedFlipY      = prefs.getBool("led_flipy", cfgLedFlipY);
    cfgLedOrder      = prefs.getInt("led_ord",   cfgLedOrder);
    cfgLedFont       = prefs.getInt("led_font",  cfgLedFont);
    cfgLedValueColor = prefs.getInt("led_vcol",  cfgLedValueColor);
    cfgLedAlwaysOn   = prefs.getBool("led_aon",  cfgLedAlwaysOn);
    cfgLedText       = prefs.getString("led_txt", cfgLedText);

    cfgMicsR0        = prefs.getFloat("cal_r0", cfgMicsR0);
    cfgMq7R0         = prefs.getFloat("mq7_r0", cfgMq7R0);
    cfgPmOffset      = prefs.getFloat("cal_pm", cfgPmOffset);

    cfgPmRhCorr      = prefs.getBool("pm_rh_en", cfgPmRhCorr);
    cfgPmRhThresh    = prefs.getFloat("pm_rh_th", cfgPmRhThresh);
    cfgPmRhCoef      = prefs.getFloat("pm_rh_coef", cfgPmRhCoef);

    cfgBoxRhWarn     = prefs.getFloat("rh_warn",  cfgBoxRhWarn);
    cfgBoxRhAlarm    = prefs.getFloat("rh_alarm", cfgBoxRhAlarm);

    cfgPirEnabled    = prefs.getBool("pir_en",   cfgPirEnabled);
    cfgPirInverted   = prefs.getBool("pir_inv",  cfgPirInverted);
    cfgPirHoldMs     = prefs.getInt("pir_hold",  cfgPirHoldMs);
    cfgPirDebounceMs = prefs.getInt("pir_deb",   cfgPirDebounceMs);

    // Pojemniki (metadane) - zapisane jako JSON pod kluczem "boxes"
    boxes_defaults();
    String boxesJson = prefs.getString("boxes", "");
    if (boxesJson.length()) boxes_from_json(boxesJson.c_str());

    LOG_I("Ustawienia wczytane z NVS (WiFi SSID: %s%s, MQTT: %s)",
          cfgWifiSsid.length() ? cfgWifiSsid.c_str() : "<puste>",
          cfgWifiSsid.length() ? "" : " -> tryb AP",
          cfgMqttHost.length() ? cfgMqttHost.c_str() : "<wylaczone>");
    if (cfgWifiStatic) {
        LOG_I("Siec: staly adres IP %s (maska %s, brama %s, DNS %s)",
              cfgWifiIp.c_str(), cfgWifiMask.c_str(), cfgWifiGw.c_str(), cfgWifiDns.c_str());
    }
    prefs.end();
}

void nvs_save() {
    if (!prefs.begin("filament", false)) return;
    prefs.putString("w_ssid", cfgWifiSsid);
    prefs.putString("w_pass", cfgWifiPass);
    prefs.putString("host",   cfgHostname);
    prefs.putBool("w_static", cfgWifiStatic);
    prefs.putString("w_ip",   cfgWifiIp);
    prefs.putString("w_mask", cfgWifiMask);
    prefs.putString("w_gw",   cfgWifiGw);
    prefs.putString("w_dns",  cfgWifiDns);
    prefs.putString("m_host", cfgMqttHost);
    prefs.putInt("m_port",    cfgMqttPort);
    prefs.putString("m_user", cfgMqttUser);
    prefs.putString("m_pass", cfgMqttPass);
    prefs.putString("m_pref", cfgMqttPrefix);
    prefs.putString("z2m",    cfgZ2mTopic);
    prefs.putBool("box_meta", cfgBoxMetaFromNode);
    prefs.putString("ha_url",   cfgHaUrl);
    prefs.putString("ha_token", cfgHaToken);
    prefs.putString("ha_temp",  cfgHaTempEntity);
    prefs.putString("ha_hum",   cfgHaHumEntity);
    prefs.putBool("air_en",     cfgAirEnabled);
    prefs.putString("air_topic", cfgAirTopic);
    prefs.putInt("led_cnt",   cfgLedCount);
    prefs.putInt("led_bri",   cfgLedBrightness);
    prefs.putInt("led_ma",    cfgLedMaxMa);
    prefs.putBool("led_mat",  cfgLedMatrix);
    prefs.putInt("led_mode",  cfgLedMode);
    prefs.putInt("led_spd",   cfgLedSpeed);
    prefs.putInt("led_lay",   cfgLedLayout);
    prefs.putInt("led_rot",   cfgLedRotate);
    prefs.putBool("led_flipy", cfgLedFlipY);
    prefs.putInt("led_ord",   cfgLedOrder);
    prefs.putInt("led_font",  cfgLedFont);
    prefs.putInt("led_vcol",  cfgLedValueColor);
    prefs.putBool("led_aon",  cfgLedAlwaysOn);
    prefs.putString("led_txt", cfgLedText);
    prefs.putFloat("cal_r0",  cfgMicsR0);
    prefs.putFloat("mq7_r0",  cfgMq7R0);
    prefs.putFloat("cal_pm",  cfgPmOffset);
    prefs.putBool("pm_rh_en",  cfgPmRhCorr);
    prefs.putFloat("pm_rh_th", cfgPmRhThresh);
    prefs.putFloat("pm_rh_coef", cfgPmRhCoef);
    prefs.putFloat("rh_warn",  cfgBoxRhWarn);
    prefs.putFloat("rh_alarm", cfgBoxRhAlarm);
    prefs.putBool("pir_en",    cfgPirEnabled);
    prefs.putBool("pir_inv",   cfgPirInverted);
    prefs.putInt("pir_hold",   cfgPirHoldMs);
    prefs.putInt("pir_deb",    cfgPirDebounceMs);
    prefs.putString("boxes",   boxes_to_json());
    prefs.end();
    LOG_I("Ustawienia zapisane do NVS");
}

void nvs_factory_reset() {
    if (!prefs.begin("filament", false)) return;
    prefs.clear();
    prefs.end();
    cfgWifiSsid = ""; cfgWifiPass = "";
    cfgWifiStatic = true;
    cfgWifiIp = "192.168.1.141"; cfgWifiMask = "255.255.255.0";
    cfgWifiGw = "192.168.1.1";  cfgWifiDns = "192.168.1.1";
    cfgMqttHost = "192.168.1.14"; cfgMqttPort = MQTT_PORT_DEFAULT;
    cfgMqttUser = ""; cfgMqttPass = ""; cfgMqttPrefix = MQTT_PREFIX_DEFAULT;
    cfgZ2mTopic = Z2M_TOPIC_DEFAULT; cfgBoxMetaFromNode = true;
    cfgHaUrl = "http://192.168.1.14:8123"; cfgHaToken = "";
    cfgHaTempEntity = "sensor.temperature_humidity_sensor_5_temperatura";
    cfgHaHumEntity  = "sensor.temperature_humidity_sensor_5_wilgotnosc";
    cfgAirEnabled = true; cfgAirTopic = "stacja_powietrza/air";
    cfgLedCount = LED_COUNT_DEFAULT; cfgLedBrightness = LED_BRIGHT_DEFAULT; cfgLedMaxMa = LED_MAX_MA_DEFAULT;
    cfgLedMatrix = true; cfgLedMode = 0; cfgLedSpeed = 5; cfgLedLayout = 0; cfgLedRotate = 0;
    cfgLedFlipY = false;
    cfgLedOrder = 0;
    cfgLedFont = 0; cfgLedValueColor = 0;
    cfgLedAlwaysOn = true; cfgLedText = "";
    cfgMicsR0 = 0; cfgMq7R0 = 0; cfgPmOffset = 0;
    cfgPmRhCorr = true; cfgPmRhThresh = PM_RH_THRESH_DEFAULT; cfgPmRhCoef = PM_RH_COEF_DEFAULT;
    cfgBoxRhWarn = BOX_RH_WARN; cfgBoxRhAlarm = BOX_RH_ALARM;
    cfgPirEnabled = true; cfgPirInverted = false; cfgPirHoldMs = 60000; cfgPirDebounceMs = 100;
    boxes_defaults();
    nvs_save();
}

// ------------------------------------------------------------------------------
String nvs_export_json() {
    JsonDocument doc;
    doc["wifi_ssid"] = cfgWifiSsid;
    doc["wifi_pass"] = cfgWifiPass;
    doc["hostname"]  = cfgHostname;
    doc["wifi_static"] = cfgWifiStatic;
    doc["wifi_ip"]   = cfgWifiIp;
    doc["wifi_mask"] = cfgWifiMask;
    doc["wifi_gw"]   = cfgWifiGw;
    doc["wifi_dns"]  = cfgWifiDns;
    doc["mqtt_host"] = cfgMqttHost;
    doc["mqtt_port"] = cfgMqttPort;
    doc["mqtt_user"] = cfgMqttUser;
    doc["mqtt_pass"] = cfgMqttPass;
    doc["mqtt_prefix"] = cfgMqttPrefix;
    doc["z2m_topic"]  = cfgZ2mTopic;
    doc["box_meta_from_node"] = cfgBoxMetaFromNode;
    doc["ha_url"]        = cfgHaUrl;
    doc["ha_token"]      = cfgHaToken;
    doc["ha_temp_entity"] = cfgHaTempEntity;
    doc["ha_hum_entity"]  = cfgHaHumEntity;
    doc["air_enabled"] = cfgAirEnabled;
    doc["air_topic"]   = cfgAirTopic;
    doc["led_count"]  = cfgLedCount;
    doc["led_brightness"] = cfgLedBrightness;
    doc["led_max_ma"] = cfgLedMaxMa;
    doc["led_matrix"] = cfgLedMatrix;
    doc["led_mode"]   = cfgLedMode;
    doc["led_speed"]  = cfgLedSpeed;
    doc["led_layout"] = cfgLedLayout;
    doc["led_rotate"] = cfgLedRotate;
    doc["led_flip_y"] = cfgLedFlipY;
    doc["led_order"]  = cfgLedOrder;
    doc["led_font"]   = cfgLedFont;
    doc["led_value_color"] = cfgLedValueColor;
    doc["led_always_on"] = cfgLedAlwaysOn;
    doc["led_text"]   = cfgLedText;
    doc["mics_r0"]    = cfgMicsR0;
    doc["mq7_r0"]     = cfgMq7R0;
    doc["pm_offset"]  = cfgPmOffset;
    doc["pm_rh_corr"]  = cfgPmRhCorr;
    doc["pm_rh_thresh"] = cfgPmRhThresh;
    doc["pm_rh_coef"]   = cfgPmRhCoef;
    doc["box_rh_warn"]  = cfgBoxRhWarn;
    doc["box_rh_alarm"] = cfgBoxRhAlarm;
    doc["pir_enabled"]  = cfgPirEnabled;
    doc["pir_inverted"] = cfgPirInverted;
    doc["pir_hold_ms"]  = cfgPirHoldMs;
    doc["pir_debounce_ms"] = cfgPirDebounceMs;
    JsonArray bx = doc["boxes"].to<JsonArray>();
    for (int i = 0; i < BOX_COUNT; i++) {
        JsonObject o = bx.add<JsonObject>();
        o["name"] = boxMeta[i].name;
        o["type"] = boxMeta[i].type;
        o["color"] = boxMeta[i].color;
        o["brand"] = boxMeta[i].brand;
        o["target_rh"] = boxMeta[i].targetRh;
    }
    String out;
    serializeJson(doc, out);
    return out;
}

bool nvs_import_json(const String& json) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) { LOG_E("Import ustawien: bledny JSON (%s)", err.c_str()); return false; }
    if (doc["wifi_ssid"].is<const char*>())  cfgWifiSsid = doc["wifi_ssid"].as<const char*>();
    if (doc["wifi_pass"].is<const char*>())  cfgWifiPass = doc["wifi_pass"].as<const char*>();
    if (doc["hostname"].is<const char*>())   cfgHostname = doc["hostname"].as<const char*>();
    if (doc["wifi_static"].is<bool>())       cfgWifiStatic = doc["wifi_static"].as<bool>();
    if (doc["wifi_ip"].is<const char*>())    cfgWifiIp = doc["wifi_ip"].as<const char*>();
    if (doc["wifi_mask"].is<const char*>())  cfgWifiMask = doc["wifi_mask"].as<const char*>();
    if (doc["wifi_gw"].is<const char*>())    cfgWifiGw = doc["wifi_gw"].as<const char*>();
    if (doc["wifi_dns"].is<const char*>())   cfgWifiDns = doc["wifi_dns"].as<const char*>();
    if (doc["mqtt_host"].is<const char*>())  cfgMqttHost = doc["mqtt_host"].as<const char*>();
    if (doc["mqtt_port"].is<int>())          cfgMqttPort = doc["mqtt_port"].as<int>();
    if (doc["mqtt_user"].is<const char*>())  cfgMqttUser = doc["mqtt_user"].as<const char*>();
    if (doc["mqtt_pass"].is<const char*>())  cfgMqttPass = doc["mqtt_pass"].as<const char*>();
    if (doc["mqtt_prefix"].is<const char*>())cfgMqttPrefix = doc["mqtt_prefix"].as<const char*>();
    if (doc["z2m_topic"].is<const char*>())  cfgZ2mTopic = doc["z2m_topic"].as<const char*>();
    if (doc["box_meta_from_node"].is<bool>()) cfgBoxMetaFromNode = doc["box_meta_from_node"].as<bool>();
    if (doc["ha_url"].is<const char*>())         cfgHaUrl = doc["ha_url"].as<const char*>();
    if (doc["ha_token"].is<const char*>())       cfgHaToken = doc["ha_token"].as<const char*>();
    if (doc["ha_temp_entity"].is<const char*>()) cfgHaTempEntity = doc["ha_temp_entity"].as<const char*>();
    if (doc["ha_hum_entity"].is<const char*>())  cfgHaHumEntity = doc["ha_hum_entity"].as<const char*>();
    if (doc["air_enabled"].is<bool>())         cfgAirEnabled = doc["air_enabled"].as<bool>();
    if (doc["air_topic"].is<const char*>())    cfgAirTopic = doc["air_topic"].as<const char*>();
    if (doc["led_count"].is<int>())          cfgLedCount = doc["led_count"].as<int>();
    if (doc["led_brightness"].is<int>())     cfgLedBrightness = doc["led_brightness"].as<int>();
    if (doc["led_max_ma"].is<int>())         cfgLedMaxMa = doc["led_max_ma"].as<int>();
    if (doc["led_matrix"].is<bool>())        cfgLedMatrix = doc["led_matrix"].as<bool>();
    if (doc["led_mode"].is<int>())           cfgLedMode = doc["led_mode"].as<int>();
    if (doc["led_speed"].is<int>())          cfgLedSpeed = doc["led_speed"].as<int>();
    if (doc["led_layout"].is<int>())         cfgLedLayout = doc["led_layout"].as<int>();
    if (doc["led_rotate"].is<int>())         cfgLedRotate = doc["led_rotate"].as<int>();
    if (doc["led_flip_y"].is<bool>())        cfgLedFlipY = doc["led_flip_y"].as<bool>();
    if (doc["led_order"].is<int>())          cfgLedOrder = doc["led_order"].as<int>();
    if (doc["led_font"].is<int>())           cfgLedFont = doc["led_font"].as<int>();
    if (doc["led_value_color"].is<int>())    cfgLedValueColor = doc["led_value_color"].as<int>();
    if (doc["led_always_on"].is<bool>())     cfgLedAlwaysOn = doc["led_always_on"].as<bool>();
    if (doc["led_text"].is<const char*>())   cfgLedText = doc["led_text"].as<const char*>();
    if (doc["mics_r0"].is<float>())          cfgMicsR0 = doc["mics_r0"].as<float>();
    if (doc["mq7_r0"].is<float>())           cfgMq7R0 = doc["mq7_r0"].as<float>();
    if (doc["pm_offset"].is<float>())        cfgPmOffset = doc["pm_offset"].as<float>();
    if (doc["pm_rh_corr"].is<bool>())        cfgPmRhCorr = doc["pm_rh_corr"].as<bool>();
    if (doc["pm_rh_thresh"].is<float>())     cfgPmRhThresh = doc["pm_rh_thresh"].as<float>();
    if (doc["pm_rh_coef"].is<float>())       cfgPmRhCoef = doc["pm_rh_coef"].as<float>();
    if (doc["box_rh_warn"].is<float>())      cfgBoxRhWarn = doc["box_rh_warn"].as<float>();
    if (doc["box_rh_alarm"].is<float>())     cfgBoxRhAlarm = doc["box_rh_alarm"].as<float>();
    if (doc["pir_enabled"].is<bool>())       cfgPirEnabled = doc["pir_enabled"].as<bool>();
    if (doc["pir_inverted"].is<bool>())      cfgPirInverted = doc["pir_inverted"].as<bool>();
    if (doc["pir_hold_ms"].is<int>())        cfgPirHoldMs = doc["pir_hold_ms"].as<int>();
    if (doc["pir_debounce_ms"].is<int>())    cfgPirDebounceMs = doc["pir_debounce_ms"].as<int>();
    if (doc["boxes"].is<JsonArray>()) {
        JsonArray arr = doc["boxes"].as<JsonArray>();
        for (int i = 0; i < BOX_COUNT && i < (int)arr.size(); i++) {
            JsonObject o = arr[i];
            if (o["name"].is<const char*>())  strncpy(boxMeta[i].name,  o["name"].as<const char*>(),  sizeof(boxMeta[i].name)  - 1);
            if (o["type"].is<const char*>())  strncpy(boxMeta[i].type,  o["type"].as<const char*>(),  sizeof(boxMeta[i].type)  - 1);
            if (o["color"].is<const char*>()) strncpy(boxMeta[i].color, o["color"].as<const char*>(), sizeof(boxMeta[i].color) - 1);
            if (o["brand"].is<const char*>()) strncpy(boxMeta[i].brand, o["brand"].as<const char*>(), sizeof(boxMeta[i].brand) - 1);
            if (o["target_rh"].is<float>())   boxMeta[i].targetRh = o["target_rh"].as<float>();
        }
    }
    nvs_save();
    return true;
}
