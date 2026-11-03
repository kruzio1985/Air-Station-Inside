#include "drv_tvoc.h"
#include "pins.h"
#include "config.h"
#include "syslog.h"

static HardwareSerial* tvocSerial = nullptr;
static TvocData s_data;

void tvoc_begin() {
    tvocSerial = new HardwareSerial(1);   // UART1
    tvocSerial->begin(TVOC_UART_BAUD, SERIAL_8N1, PIN_TVOC_RX, PIN_TVOC_TX);
    LOG_I("TVOC30712: UART1 %d b/s (RX=%d, TX=%d)", TVOC_UART_BAUD, PIN_TVOC_RX, PIN_TVOC_TX);
}

void tvoc_loop() {
    if (!tvocSerial) return;
    static uint8_t buf[11];
    static int     idx = 0;

    // DEBUG: zlicza surowe bajty i zapamietuje pierwsze, zeby zdiagnozowac
    // czy dane w ogole docieraja na GPIO15 (i jak wygladaja).
    static uint32_t s_rxCount = 0;
    static uint8_t  s_first[24];
    static int      s_firstN = 0;
    static uint32_t s_lastDbg = 0;

    while (tvocSerial->available()) {
        uint8_t c = tvocSerial->read();
        s_rxCount++;
        if (s_firstN < 24) s_first[s_firstN++] = c;
        if (idx == 0 && c != 0xFE) continue;
        buf[idx++] = c;
        if (idx >= 11) {
            if (buf[0] == 0xFE && buf[10] == 0x16) {
                // Waveshare 30712 bywa niespojny: jedne egzemplarze licza sume
                // bajtow 0..8 (z naglowkiem FE), inne 1..8 (bez naglowka).
                // Akceptujemy obie wersje +-1 (pojedynczy blad bitu na UART).
                uint16_t s0 = 0;
                for (int i = 0; i < 9; i++) s0 += buf[i];
                uint16_t s1 = s0 - buf[0];
                uint8_t cs0 = s0 & 0xFF, cs1 = s1 & 0xFF;
                int d0 = (int)buf[9] - cs0; if (d0 < -128) d0 += 256; if (d0 > 127) d0 -= 256;
                int d1 = (int)buf[9] - cs1; if (d1 < -128) d1 += 256; if (d1 > 127) d1 -= 256;
                bool ok0 = (d0 == 0 || d0 == 1 || d0 == -1);
                bool ok1 = (d1 == 0 || d1 == 1 || d1 == -1);
                if (ok0 || ok1) {
                    s_data.co2   = (buf[3] << 8) | buf[4];
                    s_data.ch2o  = (buf[5] << 8) | buf[6];
                    s_data.tvoc  = ((buf[7] << 8) | buf[8]) / 1000.0f;
                    s_data.level = buf[1];
                    s_data.valid = true;
                    s_data.lastMs = millis();

                    static uint32_t lastLog = 0;
                    if (millis() - lastLog > 30000) {
                        lastLog = millis();
                        LOG_I("TVOC: CO2=%.0f ppm, CH2O=%.0f ppb, TVOC=%.3f ppm, poziom=%d",
                              s_data.co2, s_data.ch2o, s_data.tvoc, s_data.level);
                    }
                } else {
                    static uint32_t lastBad = 0;
                    if (millis() - lastBad > 5000) {
                        lastBad = millis();
                        char hx[40]; int p = 0;
                        for (int i = 0; i < 11; i++) p += snprintf(hx + p, sizeof(hx) - p, "%02X ", buf[i]);
                        LOG_E("TVOC: zla suma kontrolna: %s", hx);
                    }
                }
            }
            idx = 0;
        }
    }

    if (millis() - s_lastDbg > 10000) {
        s_lastDbg = millis();
        if (s_rxCount > 0) {
            char hex[80]; int p = 0;
            for (int i = 0; i < s_firstN; i++) p += snprintf(hex + p, sizeof(hex) - p, "%02X ", s_first[i]);
            LOG_I("TVOC dbg: %lu B na GPIO%d, pierwsze: %s", (unsigned long)s_rxCount, PIN_TVOC_RX, hex);
        } else {
            LOG_I("TVOC dbg: 0 B w ciagu 10 s (cisza na GPIO%d)", PIN_TVOC_RX);
        }
        s_rxCount = 0;
        s_firstN = 0;
    }
}

TvocData tvoc_get() { return s_data; }
