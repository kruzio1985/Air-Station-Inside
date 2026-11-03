#include "drv_pms7003.h"
#include "pins.h"
#include "config.h"
#include "syslog.h"

static HardwareSerial* pmsSerial = nullptr;
static PmsData s_data;

static bool parse_frame(const uint8_t* f, int len) {
    // f zaczyna sie od 0x42 0x4D, len = 32
    if (len != 32 || f[0] != 0x42 || f[1] != 0x4D) return false;
    uint16_t sum = 0;
    for (int i = 0; i < 30; i++) sum += f[i];
    uint16_t cks = ((uint16_t)f[30] << 8) | f[31];
    if (sum != cks) return false;

    auto u16 = [&](int i) { return (uint16_t)((f[i] << 8) | f[i + 1]); };

    // Atmospheric environment (bajty 10-15) - BEZ zadnego przeliczania temp/RH.
    s_data.pm1   = u16(10);  // PM1.0 atmospheric
    s_data.pm25  = u16(12);  // PM2.5 atmospheric
    s_data.pm10  = u16(14);  // PM10  atmospheric
    s_data.valid = true;
    s_data.lastMs = millis();
    return true;
}

void pms_begin() {
    pmsSerial = new HardwareSerial(2);   // UART2
    pmsSerial->begin(PMS_UART_BAUD, SERIAL_8N1, PIN_PMS_RX, PIN_PMS_TX);
    LOG_I("PMS7003: UART2 %d b/s (RX=%d, TX=%d)", PMS_UART_BAUD, PIN_PMS_RX, PIN_PMS_TX);
}

void pms_loop() {
    if (!pmsSerial) return;
    static uint8_t buf[32];
    static int     idx = 0;

    while (pmsSerial->available()) {
        uint8_t c = pmsSerial->read();
        // synchronizacja po naglowku 0x42 0x4D
        if (idx == 0 && c != 0x42) continue;
        if (idx == 1 && c != 0x4D) { idx = (c == 0x42) ? 1 : 0; continue; }
        buf[idx++] = c;
        if (idx >= 32) {
            if (parse_frame(buf, 32)) {
                // LOG co 30 s, zeby nie spamowac
                static uint32_t lastLog = 0;
                if (millis() - lastLog > 30000) {
                    lastLog = millis();
                    LOG_I("PMS (atmosferyczne): PM1.0=%.0f PM2.5=%.0f PM10=%.0f ug/m3",
                          s_data.pm1, s_data.pm25, s_data.pm10);
                }
            } else {
                // bledna suma - kolejne ramki i tak przyjda
            }
            idx = 0;
        }
    }
}

PmsData pms_get() { return s_data; }
