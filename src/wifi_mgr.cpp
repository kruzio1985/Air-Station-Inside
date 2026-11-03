#include "wifi_mgr.h"
#include "config.h"
#include "nvs_store.h"
#include "syslog.h"
#include <WiFi.h>
#include <WiFiAP.h>
#include <ESPmDNS.h>
#include <time.h>

#define NTP_SERVER  "pool.ntp.org"
#define TZ_STRING   "CET-1CEST,M3.5.0,M10.5.0/3"

static bool   s_apMode = false;
static bool   s_timeSet = false;
static uint32_t s_lastNtp = 0;

// Staly adres IP (o ile wlaczony i poprawny). Wywolywac PRZED WiFi.begin().
static void apply_static_ip() {
    if (!cfgWifiStatic) return;
    IPAddress ip, gw, mask, dns;
    if (ip.fromString(cfgWifiIp) && gw.fromString(cfgWifiGw) && mask.fromString(cfgWifiMask)) {
        bool ok;
        if (dns.fromString(cfgWifiDns)) ok = WiFi.config(ip, gw, mask, dns);
        else                            ok = WiFi.config(ip, gw, mask);
        if (ok) LOG_I("WiFi: staly IP %s / maska %s / brama %s", ip.toString().c_str(), mask.toString().c_str(), gw.toString().c_str());
        else    LOG_W("WiFi: nie udalo sie ustawic stalego IP - zostaje DHCP");
    } else {
        LOG_W("WiFi: bledny adres IP w ustawieniach - zostaje DHCP");
    }
}

void wifi_begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(cfgHostname.c_str());
    WiFi.persistent(false);

    if (cfgWifiSsid.length() > 0) {
        LOG_I("WiFi: lacze z %s ...", cfgWifiSsid.c_str());
        apply_static_ip();
        WiFi.begin(cfgWifiSsid.c_str(), cfgWifiPass.c_str());
        unsigned long t0 = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - t0) < WIFI_CONNECT_MS) {
            delay(200);
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        LOG_I("WiFi: polaczono, IP=%s", WiFi.localIP().toString().c_str());
        s_apMode = false;
    } else {
        LOG_W("WiFi: brak polaczenia -> punkt dostepowy %s", AP_SSID_DEFAULT);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID_DEFAULT, AP_PASS_DEFAULT);
        s_apMode = true;
        LOG_I("AP: %s, IP=%s", AP_SSID_DEFAULT, WiFi.softAPIP().toString().c_str());
    }

    configTime(0, 0, NTP_SERVER);
    setenv("TZ", TZ_STRING, 1);
    tzset();

    if (MDNS.begin(cfgHostname.c_str())) {
        MDNS.addService("http", "tcp", 80);
        LOG_I("mDNS: http://%s.local", cfgHostname.c_str());
    }
}

void wifi_loop() {
    if (!s_apMode && WiFi.status() != WL_CONNECTED) {
        // proba ponownego polaczenia co 30 s
        static uint32_t last = 0;
        if (millis() - last > 30000) {
            last = millis();
            LOG_W("WiFi: utracone polaczenie, ponawiam...");
            WiFi.disconnect();
            apply_static_ip();
            WiFi.begin(cfgWifiSsid.c_str(), cfgWifiPass.c_str());
        }
    }
    // NTP co godzine
    if (!s_apMode && WiFi.status() == WL_CONNECTED && (millis() - s_lastNtp) > 3600000) {
        s_lastNtp = millis();
        configTime(0, 0, NTP_SERVER);
        setenv("TZ", TZ_STRING, 1);
        tzset();
        struct tm t;
        if (getLocalTime(&t, 100)) {
            s_timeSet = true;
            LOG_I("NTP: czas zsynchronizowany %04d-%02d-%02d %02d:%02d",
                  t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min);
        }
    }
}

bool wifi_connected() { return WiFi.status() == WL_CONNECTED; }

String wifi_mode_label() {
    if (s_apMode) return "AP";
    if (WiFi.status() == WL_CONNECTED) return "STA";
    return "brak";
}
