#include "sd_card.h"
#include "app.h"
#include "pins.h"
#include "config.h"
#include "syslog.h"
#include "agg.h"
#include <SPI.h>
#include <SD.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

static bool s_ready = false;
static uint32_t s_lastWrite = 0;

static String csv_filename() {
    struct tm t;
    if (!getLocalTime(&t, 100)) { t.tm_year = 0; t.tm_mon = 0; t.tm_mday = 1; }
    char buf[40];
    snprintf(buf, sizeof(buf), "%s/%s-%04d-%02d.csv", SD_LOG_DIR, SD_CSV_FILENAME,
             t.tm_year + 1900, t.tm_mon + 1);
    return String(buf);
}

static void write_header(File& f) {
    f.println("timestamp,pm1,pm25,pm10,co2,tvoc,ch2o,temp,hum");
}

static void fmt_ts(uint32_t unix, char* buf, size_t len) {
    if (unix >= 1600000000UL) {
        time_t tt = (time_t)unix;
        struct tm* t = localtime(&tt);
        if (t) {
            snprintf(buf, len, "%04d-%02d-%02d %02d:%02d:%02d",
                     t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                     t->tm_hour, t->tm_min, t->tm_sec);
            return;
        }
    }
    snprintf(buf, len, "----");
}

static void fval(File& f, float v, int dec) {
    f.print(',');
    if (!isnan(v)) f.print(v, dec);
}

static void write_rec(File& f, const MinuteRec& r) {
    char ts[32];
    fmt_ts(r.ts, ts, sizeof(ts));
    f.print(ts);
    fval(f, r.pm1, 1);
    fval(f, r.pm25, 1);
    fval(f, r.pm10, 1);
    fval(f, r.co2, 0);
    fval(f, r.tvoc, 3);
    fval(f, r.ch2o, 0);
    fval(f, r.temp, 1);
    fval(f, r.hum, 0);
    f.println();
}

// Parsowanie "YYYY-MM-DD HH:MM:SS" -> unix (0 gdy blad / brak czasu).
static uint32_t parse_ts(const char* s) {
    int Y = 0, M = 0, D = 0, h = 0, m = 0, sec = 0;
    if (sscanf(s, "%d-%d-%d %d:%d:%d", &Y, &M, &D, &h, &m, &sec) != 6) return 0;
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = Y - 1900;
    t.tm_mon  = M - 1;
    t.tm_mday = D;
    t.tm_hour = h;
    t.tm_min  = m;
    t.tm_sec  = sec;
    t.tm_isdst = -1;
    time_t tt = mktime(&t);
    return (tt > 0) ? (uint32_t)tt : 0;
}

// Odtworzenie dzisiejszego pliku CSV do ring-buffera 24h (po restarcie).
static void sd_restore_today() {
    String fname = csv_filename();
    File f = SD.open(fname, FILE_READ);
    if (!f) return;
    f.readStringUntil('\n');   // pomijamy naglowek
    int n = 0;
    while (f.available() && n < AGG_CAP) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;
        char buf[256];
        line.toCharArray(buf, sizeof(buf));
        MinuteRec r;
        r.ts = 0;
        r.pm1 = r.pm25 = r.pm10 = NAN;
        r.co2 = r.tvoc = r.ch2o = NAN;
        r.temp = r.hum = NAN;
        char* tok = strtok(buf, ",");
        int idx = 0;
        while (tok) {
            if (idx == 0) {
                r.ts = parse_ts(tok);          // kolumna 0 = "YYYY-MM-DD HH:MM:SS"
            } else if (idx >= 1 && idx <= 8) {
                float v = (tok[0] == '\0') ? NAN : (float)atof(tok);
                switch (idx) {
                    case 1: r.pm1  = v; break;
                    case 2: r.pm25 = v; break;
                    case 3: r.pm10 = v; break;
                    case 4: r.co2  = v; break;
                    case 5: r.tvoc = v; break;
                    case 6: r.ch2o = v; break;
                    case 7: r.temp = v; break;
                    case 8: r.hum  = v; break;
                }
            }
            tok = strtok(NULL, ",");
            idx++;
        }
        agg_restore_rec(r);
        n++;
    }
    f.close();
    if (n > 0) {
        agg_restore_done();
        LOG_I("SD: odtworzono %d rekordow minutowych z %s", n, fname.c_str());
    }
}

void sd_begin() {
    SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    if (SD.begin(PIN_SD_CS, SPI, 4000000)) {
        if (!SD.exists(SD_LOG_DIR)) {
            if (SD.mkdir(SD_LOG_DIR)) LOG_I("SD: utworzono %s", SD_LOG_DIR);
        }
        uint64_t total = SD.totalBytes() / (1024 * 1024);
        s_ready = true;
        LOG_I("SD: OK (CS=%d, SCK=%d, MOSI=%d, MISO=%d), %llu MB",
              PIN_SD_CS, PIN_SD_SCK, PIN_SD_MOSI, PIN_SD_MISO, (unsigned long long)total);
        sd_restore_today();
    } else {
        s_ready = false;
        LOG_E("SD: brak karty / blad inicjalizacji");
    }
}

void sd_loop() {
    if (!s_ready) {
        // Karta wlozona po starcie? Proba ponownego wykrycia co 5 s.
        static uint32_t s_lastTry = 0;
        if (millis() - s_lastTry < 5000) return;
        s_lastTry = millis();
        if (SD.begin(PIN_SD_CS, SPI, 4000000)) {
            if (!SD.exists(SD_LOG_DIR)) SD.mkdir(SD_LOG_DIR);
            uint64_t total = SD.totalBytes() / (1024 * 1024);
            s_ready = true;
            s_lastWrite = 0;
            LOG_I("SD: OK (CS=%d, SCK=%d, MOSI=%d, MISO=%d), %llu MB",
                  PIN_SD_CS, PIN_SD_SCK, PIN_SD_MOSI, PIN_SD_MISO, (unsigned long long)total);
        } else {
            LOG_W("SD: nadal brak karty (proba ponownego wykrycia za 5 s)");
        }
        return;
    }
    uint32_t now = millis();
    if (now - s_lastWrite < SD_LOG_INTERVAL_MS) return;
    if (agg_pending() == 0) { s_lastWrite = now; return; }
    s_lastWrite = now;

    String fname = csv_filename();
    bool exists = SD.exists(fname);

    File f = SD.open(fname, FILE_APPEND);
    if (!f) { LOG_E("SD: nie moge otworzyc %s", fname.c_str()); return; }

    if (!exists) write_header(f);

    uint32_t n = agg_pending();
    for (uint32_t i = 0; i < n; i++) {
        MinuteRec r;
        if (agg_pending_get(i, r)) write_rec(f, r);
    }
    f.close();
    agg_pending_mark(n);
    LOG_I("SD: zapisano %lu rekordow minutowych -> %s", (unsigned long)n, fname.c_str());
}

bool sd_ready() { return s_ready; }
