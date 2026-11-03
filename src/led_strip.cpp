// =============================================================================
//  LED: pasek WS2812B albo matryca LED 8x8 (64 diody WS2812B).
//  Tryby: kolor jakosci powietrza (domyslny) + efekty: tecza, oddychanie,
//  kometa, iskry, przewijany tekst (zegar / wlasny tekst).
//  Matryca jest konfigurowalna (uklad serpentine/progressive, obrot).
// =============================================================================
#include "led_strip.h"
#include "app.h"
#include "pins.h"
#include "config.h"
#include "nvs_store.h"
#include "sensors.h"
#include "aqi.h"
#include "syslog.h"
#include <FastLED.h>
#include <time.h>
#include <string.h>

#define LED_MAX_LEDS 300
#define MATRIX_W 8
#define MATRIX_H 8
#define MATRIX_N (MATRIX_W * MATRIX_H)

// --- Tryby efektow -------------------------------------------------------------
#define LED_MODE_AIR     0   // kolor jakosci powietrza
#define LED_MODE_RAINBOW 1   // tecza
#define LED_MODE_BREATHE 2   // oddychanie (kolor powietrza)
#define LED_MODE_COMET   3   // kometa
#define LED_MODE_SPARKLE 4   // iskry
#define LED_MODE_SCROLL  5   // przewijany tekst
#define LED_MODE_LEVEL   6   // pasek poziomu jakosci powietrza (AQI)
#define LED_MODE_NUM     7   // liczba PM2.5 (2 cyfry)
#define LED_MODE_SMILEY  8   // buzka wg jakosci powietrza
#define LED_MODE_AQI     9   // cyfry AQI (2 cyfry / przewijane 3 cyfry)
#define LED_MODE_MATRIX  10  // matrix rain (spadajace kropki)
#define LED_MODE_FIRE    11  // ogien
#define LED_MODE_PLASMA  12  // plasma
#define LED_MODE_BOUNCE  13  // bouncing ball
#define LED_MODE_AUTO    14  // auto: cykl ekranow z danymi czujnikow
#define LED_MODE_ICONS   15  // pokaz slajdow ikon pixel-art
#define LED_MODE_COUNT   16

// --- Czcionka 3x5 (3 kolumny po 5 bitow, bit0 = gorny wiersz) ------------------
// Indeksy: 0-9 cyfry, 10-35 litery A-Z, 36=':', 37='.', 38='-', 39=' '
static const uint8_t FONT[40][3] PROGMEM = {
    {0x1F,0x11,0x1F}, // 0
    {0x02,0x1F,0x00}, // 1
    {0x1D,0x15,0x17}, // 2
    {0x11,0x15,0x1F}, // 3
    {0x07,0x04,0x1F}, // 4
    {0x17,0x15,0x1D}, // 5
    {0x1F,0x15,0x1D}, // 6
    {0x01,0x13,0x07}, // 7
    {0x1F,0x15,0x1F}, // 8
    {0x17,0x15,0x1F}, // 9
    {0x1F,0x15,0x15}, // A
    {0x1F,0x15,0x13}, // B
    {0x1F,0x11,0x11}, // C
    {0x1F,0x11,0x1F}, // D
    {0x1F,0x15,0x11}, // E
    {0x1F,0x05,0x01}, // F
    {0x0E,0x11,0x17}, // G
    {0x1F,0x04,0x1F}, // H
    {0x11,0x1F,0x11}, // I
    {0x18,0x10,0x1F}, // J
    {0x1F,0x04,0x13}, // K
    {0x1F,0x00,0x00}, // L
    {0x1F,0x06,0x1F}, // M
    {0x1F,0x0E,0x1F}, // N
    {0x1F,0x11,0x1F}, // O
    {0x1F,0x05,0x07}, // P
    {0x06,0x12,0x13}, // Q
    {0x1F,0x05,0x13}, // R
    {0x17,0x15,0x1D}, // S
    {0x01,0x1F,0x01}, // T
    {0x0F,0x10,0x1F}, // U
    {0x07,0x18,0x07}, // V
    {0x1F,0x0C,0x1F}, // W
    {0x1B,0x04,0x1B}, // X
    {0x03,0x1C,0x03}, // Y
    {0x13,0x15,0x1F}, // Z
    {0x00,0x0A,0x00}, // :
    {0x00,0x00,0x10}, // .
    {0x04,0x04,0x04}, // -
    {0x00,0x00,0x00}, // spacja
};

// --- Duza czcionka cyfr 4x6 (4 kolumny x 6 bitow, bit0 = gorny wiersz) ---------
static const uint8_t FONT_4x6[10][4] PROGMEM = {
    {0x1E,0x21,0x21,0x1E}, // 0
    {0x00,0x22,0x3F,0x00}, // 1
    {0x22,0x31,0x29,0x13}, // 2
    {0x12,0x21,0x29,0x16}, // 3
    {0x18,0x0A,0x12,0x3F}, // 4
    {0x17,0x29,0x29,0x19}, // 5
    {0x1E,0x29,0x29,0x0C}, // 6
    {0x01,0x39,0x05,0x03}, // 7
    {0x1A,0x29,0x29,0x1A}, // 8
    {0x06,0x29,0x29,0x1E}, // 9
};

// --- Mini czcionka 3x3 (etykiety) ----------------------------------------------
// Indeksy: 0-9 cyfry, 10-35 litery A-Z, 36='.', 37=' ', 38='-'
static const uint8_t FONT_3x3[39][3] PROGMEM = {
    {7,5,7}, // 0
    {2,7,0}, // 1
    {5,5,7}, // 2
    {5,3,7}, // 3
    {5,2,2}, // 4
    {7,4,7}, // 5
    {7,4,7}, // 6
    {1,1,7}, // 7
    {7,5,7}, // 8
    {7,5,7}, // 9
    {7,5,7}, // A
    {7,5,7}, // B
    {7,4,7}, // C
    {7,5,7}, // D
    {7,6,7}, // E
    {7,6,4}, // F
    {7,4,5}, // G
    {5,7,5}, // H
    {7,2,7}, // I
    {1,1,6}, // J
    {5,6,5}, // K
    {4,4,7}, // L
    {7,7,5}, // M
    {7,7,5}, // N
    {7,5,7}, // O
    {7,5,4}, // P
    {7,5,6}, // Q
    {7,6,5}, // R
    {3,2,6}, // S
    {7,2,2}, // T
    {5,5,7}, // U
    {5,5,2}, // V
    {5,7,7}, // W
    {5,2,5}, // X
    {5,2,2}, // Y
    {7,2,7}, // Z
    {0,0,2}, // .
    {0,0,0}, // spacja
    {0,7,0}, // -
};

// --- Buzki 8x8 (bit0 = lewa kolumna) -------------------------------------------
static const uint8_t FACE_HAPPY[8]   PROGMEM = {0x3C,0x42,0x81,0xA5,0x81,0x81,0x42,0x3C};
static const uint8_t FACE_NEUTRAL[8] PROGMEM = {0x3C,0x42,0x81,0xA5,0x81,0x81,0x81,0x7E};
static const uint8_t FACE_SAD[8]     PROGMEM = {0x3C,0x42,0x81,0xA5,0x81,0x81,0x3C,0x42};
static const uint8_t FACE_SICK[8]    PROGMEM = {0x3C,0x42,0xA5,0x81,0xA5,0x81,0x3C,0x42};
static const uint8_t FACE_BLINK[8]   PROGMEM = {0x3C,0x42,0x81,0x81,0x81,0x81,0x42,0x3C};

// --- Ikony pixel-art 8x8 (bit0 = lewa kolumna) ---------------------------------
enum { ICON_TERM = 0, ICON_DROP, ICON_CLOUD, ICON_SUN, ICON_RAIN, ICON_HEART, ICON_GHOST, ICON_STAR, ICON_COUNT };
static const uint8_t ICONS[ICON_COUNT][8] PROGMEM = {
    {0x08,0x14,0x14,0x14,0x14,0x36,0x3E,0x1C}, // termometr
    {0x18,0x3C,0x3C,0x7E,0x7E,0xFF,0x7E,0x3C}, // kropla
    {0x3C,0x7E,0xFF,0xFF,0xFF,0x7E,0x3C,0x00}, // chmura
    {0x24,0x00,0x18,0xFF,0xFF,0x18,0x00,0x42}, // slonce
    {0x3C,0x7E,0xFF,0xFF,0x7E,0x3C,0x12,0x24}, // deszcz
    {0x66,0xFF,0xFF,0xFF,0x7E,0x3C,0x18,0x00}, // serce
    {0x7E,0xFF,0xFF,0xFF,0xFF,0xFF,0xA5,0x5A}, // duch
    {0x18,0x18,0x18,0xFF,0x7E,0x3C,0x18,0x00}, // gwiazda
};

static CRGB leds[LED_MAX_LEDS];
static CRGB s_out[LED_MAX_LEDS];   // bufor wyjsciowy (z przelozona kolejnoscia kolorow)
static int  s_ledCount = 0;
static bool s_initialized = false;
static CLEDController* s_controller = nullptr;

// tryb podgladu z www (test koloru), wygasa po ms
static uint32_t s_testColor = 0;
static uint32_t s_testUntil = 0;
static uint32_t s_dotsUntil = 0;   // diagnostyka: 4 diody (indeksy lancucha)

// stan efektu
static int      s_mode    = 0;
static int      s_speed   = 5;
static uint32_t s_lastFx  = 0;
static uint8_t  s_hue     = 0;
static int      s_cometPos = 0;
static int      s_scrollX = MATRIX_W;
static int      s_labelX  = MATRIX_W;   // pozycja przewijanej etykiety

static void led_clear();
static void led_show();
static void draw_big_glyph(char ch, int gx, CRGB color);

// --- Mapowanie XY (matryca) -----------------------------------------------------
static int led_idx(int x, int y) {
    if (!cfgLedMatrix) return x;   // pasek: tylko wspolrzedna x
    int r = ((cfgLedRotate % 4) + 4) % 4;
    int X = x, Y = y;
    switch (r) {
        case 1: X = y;              Y = MATRIX_H - 1 - x; break;  // 90 stopni CW
        case 2: X = MATRIX_W - 1 - x; Y = MATRIX_H - 1 - y; break; // 180
        case 3: X = MATRIX_W - 1 - y; Y = x; break;              // 270 CW
        default: break;
    }
    if (cfgLedFlipY) Y = MATRIX_H - 1 - Y;   // odbicie w pionie (matryca do gory nogami)
    int row = Y;
    if (cfgLedLayout == 0) {   // serpentine (zygzak)
        return (row & 1) ? (row + 1) * MATRIX_W - 1 - X : row * MATRIX_W + X;
    }
    return row * MATRIX_W + X; // progressive (wiersz po wierszu)
}

static const uint8_t* font_glyph(char c) {
    if (c >= 'a' && c <= 'z') c -= ('a' - 'A');
    if (c >= '0' && c <= '9') return FONT[c - '0'];
    if (c >= 'A' && c <= 'Z') return FONT[c - 'A' + 10];
    switch (c) {
        case ':': return FONT[36];
        case '.': return FONT[37];
        case '-': return FONT[38];
        default:  return FONT[39];
    }
}

static const uint8_t* font3_glyph(char c) {
    if (c >= 'a' && c <= 'z') c -= ('a' - 'A');
    if (c >= '0' && c <= '9') return FONT_3x3[c - '0'];
    if (c >= 'A' && c <= 'Z') return FONT_3x3[c - 'A' + 10];
    switch (c) {
        case '.': return FONT_3x3[36];
        case '-': return FONT_3x3[38];
        default:  return FONT_3x3[37];
    }
}

void led_begin() {
    led_apply_settings();
}

void led_apply_settings() {
    int n;
    if (cfgLedMatrix) n = MATRIX_N;                       // matryca 8x8 = 64
    else              n = cfgLedCount > 0 ? cfgLedCount : LED_COUNT_DEFAULT;
    if (n > LED_MAX_LEDS) n = LED_MAX_LEDS;
    s_ledCount = (PIN_RGB > 0 && n > 0) ? n : 0;

    s_mode  = (cfgLedMode >= 0 && cfgLedMode < LED_MODE_COUNT) ? cfgLedMode : 0;
    s_speed = (cfgLedSpeed >= 1 && cfgLedSpeed <= 10) ? cfgLedSpeed : 5;
    s_scrollX = MATRIX_W;

    if (s_ledCount == 0) {
        LOG_W("LED: wylaczony (brak pinu lub diod)");
        return;
    }
    if (!s_initialized) {
        s_controller = &FastLED.addLeds<WS2812B, PIN_RGB, GRB>(s_out, s_ledCount);
        s_initialized = true;
        LOG_I("LED: %s, %d diod na GPIO%d, jasnosc %d, limit %d mA, tryb %s",
              cfgLedMatrix ? "matryca 8x8" : "pasek", s_ledCount, PIN_RGB,
              cfgLedBrightness, cfgLedMaxMa, led_mode_name(s_mode));
    } else {
        // Zmiana liczby diod bez restartu.
        s_controller->setLeds(s_out, s_ledCount);
    }
    FastLED.setBrightness(led_eff_brightness());
    led_clear();
    led_show();
}

// Jasnosc po limicie pradu: biala dioda ~60 mA przy 255.
uint8_t led_eff_brightness() {
    int n = s_ledCount > 0 ? s_ledCount : (cfgLedMatrix ? MATRIX_N : cfgLedCount);
    if (n <= 0) n = 1;
    float maxByCurrent = (cfgLedMaxMa > 0)
        ? (255.0f * cfgLedMaxMa / (n * 60.0f))
        : 255.0f;
    if (maxByCurrent > 255.0f) maxByCurrent = 255.0f;
    if (maxByCurrent < 1.0f)   maxByCurrent = 1.0f;
    uint8_t b = (uint8_t)maxByCurrent;
    if (cfgLedBrightness < b) b = cfgLedBrightness;
    if (b > LED_BRIGHT_MAX) b = LED_BRIGHT_MAX;   // twardy limit 50%
    return b;
}

// Wyczysc bufor rysowania (leds[]) - FastLED.clear() dotyczy bufora wyjsciowego.
static void led_clear() {
    memset(leds, 0, s_ledCount * sizeof(CRGB));
}

// Przekopiuj leds[] do bufora wyjsciowego z przelozona kolejnoscia kolorow
// i wyslij. Rysowanie zawsze odbywa sie na leds[] (poprawny RGB).
static void led_show() {
    for (int i = 0; i < s_ledCount; i++) {
        const CRGB& c = leds[i];
        switch (cfgLedOrder) {
            case 1: s_out[i] = CRGB(c.g, c.r, c.b); break; // RGB
            case 2: s_out[i] = CRGB(c.g, c.b, c.r); break; // BGR
            case 3: s_out[i] = CRGB(c.r, c.b, c.g); break; // BRG
            case 4: s_out[i] = CRGB(c.b, c.r, c.g); break; // GBR
            case 5: s_out[i] = CRGB(c.b, c.g, c.r); break; // RBG
            default: s_out[i] = c; break;                 // GRB
        }
    }
    FastLED.show();
}

static void fill_rgb(uint32_t c, uint8_t bright) {
    if (!s_initialized || s_ledCount == 0) return;
    uint8_t r = (c >> 16) & 0xFF;
    uint8_t g = (c >> 8)  & 0xFF;
    uint8_t b = (c >> 0)  & 0xFF;
    fill_solid(leds, s_ledCount, CRGB(r, g, b));
    FastLED.setBrightness(bright);
    led_show();
}

static bool is_night() {
    struct tm t;
    if (!getLocalTime(&t, 100)) return false;
    int h = t.tm_hour;
    return (h >= 22 || h < 6);
}

// --- Progi koloru powietrza ----------------------------------------------------
static bool led_over_yellow(float pm25, float pm10, float co2, float tvoc, float ch2o) {
    return pm25 >= 35 || pm10 >= 45 || co2 >= 1000 || tvoc >= 0.5f || ch2o >= 40;
}
static bool led_over_red(float pm25, float pm10, float co2, float tvoc, float ch2o) {
    return pm25 >= 50 || pm10 >= 75 || co2 >= 1500 || tvoc >= 1.0f || ch2o >= 80;
}
static bool led_under_yellow(float pm25, float pm10, float co2, float tvoc, float ch2o) {
    return pm25 < 30 && pm10 < 40 && co2 < 900 && tvoc < 0.4f && ch2o < 30;
}
static bool led_under_red(float pm25, float pm10, float co2, float tvoc, float ch2o) {
    return pm25 < 45 && pm10 < 70 && co2 < 1400 && tvoc < 0.9f && ch2o < 70;
}

static int      s_airLevel = 0;      // 0=zielony, 1=zolty, 2=czerwony
static int      s_pending  = -1;     // poziom oczekujacy na potwierdzenie
static uint32_t s_pendingSince = 0;

// Krytyczny alarm: awaria czujnikow albo wykryty gaz (MiCS po wygrzaniu).
static bool air_alarm_active() {
    Channel* pm25c = channel_find("pm25");
    Channel* pm10c = channel_find("pm10");
    Channel* co2c  = channel_find("co2");
    Channel* coCh  = channel_find("mics_co");
    Channel* wuCh  = channel_find("mics_wu");
    bool pmOk = pm25c && pm25c->valid && pm10c && pm10c->valid;
    bool tvOk = co2c  && co2c->valid;
    bool sensorFail = (!pmOk && !tvOk);
    bool gasBad = (coCh && coCh->valid && wuCh && wuCh->valid &&
                   wuCh->value < 0.5f && coCh->value > 100.0f);
    return gasBad || sensorFail;
}

// Biezacy kolor powietrza (bez histerezy) - do efektow "oddychanie"/"tekst".
static uint32_t current_air_color() {
    Channel* pm25c = channel_find("pm25");
    Channel* pm10c = channel_find("pm10");
    Channel* co2c  = channel_find("co2");
    Channel* tvocc = channel_find("tvoc");
    Channel* ch2oc = channel_find("ch2o");
    bool pmOk = pm25c && pm25c->valid && pm10c && pm10c->valid;
    bool tvOk = co2c && co2c->valid && tvocc && tvocc->valid && ch2oc && ch2oc->valid;
    if (!pmOk && !tvOk) return 0x8E24AA;   // fioletowy - brak danych
    float pm25 = pmOk ? pm25c->value : 0;
    float pm10 = pmOk ? pm10c->value : 0;
    float co2  = tvOk ? co2c->value  : 0;
    float tvoc = tvOk ? tvocc->value : 0;
    float ch2o = tvOk ? ch2oc->value : 0;
    if (led_over_red(pm25, pm10, co2, tvoc, ch2o))    return 0xE53935;
    if (led_over_yellow(pm25, pm10, co2, tvoc, ch2o)) return 0xFFD600;
    return 0x00C853;
}

// --- Tryb: kolor jakosci powietrza (histereza 5 min + ruch) ---------------------
static void air_quality_frame(uint32_t now) {
    uint8_t full = led_eff_brightness();
    uint8_t base = is_night() ? full / 2 : full;

    Channel* pm25c = channel_find("pm25");
    Channel* pm10c = channel_find("pm10");
    Channel* co2c  = channel_find("co2");
    Channel* tvocc = channel_find("tvoc");
    Channel* ch2oc = channel_find("ch2o");

    bool pmOk = pm25c && pm25c->valid && pm10c && pm10c->valid;
    bool tvOk = co2c  && co2c->valid;

    // Oszczedzanie: wylacz bez ruchu (tylko gdy "zawsze wlaczona" jest wylaczone).
    if (!cfgLedAlwaysOn && !motion_active()) {
        fill_rgb(0x000000, 0);
        return;
    }

    // Brak danych (swiezy start) -> fioletowy.
    if (!pmOk && !tvOk) {
        fill_rgb(0x8E24AA, base);
        return;
    }

    float f_pm25 = pmOk ? pm25c->value : NAN;
    float f_pm10 = pmOk ? pm10c->value : NAN;
    float f_co2  = tvOk ? co2c->value  : NAN;
    float f_tvoc = tvOk ? tvocc->value : NAN;
    float f_ch2o = tvOk ? ch2oc->value : NAN;

    int desired;
    if (s_airLevel == 0)      desired = led_over_red(f_pm25, f_pm10, f_co2, f_tvoc, f_ch2o) ? 2
                                             : led_over_yellow(f_pm25, f_pm10, f_co2, f_tvoc, f_ch2o) ? 1 : 0;
    else if (s_airLevel == 1) desired = led_over_red(f_pm25, f_pm10, f_co2, f_tvoc, f_ch2o) ? 2
                                             : led_under_yellow(f_pm25, f_pm10, f_co2, f_tvoc, f_ch2o) ? 0 : 1;
    else                      desired = led_under_red(f_pm25, f_pm10, f_co2, f_tvoc, f_ch2o)
                                             ? (led_over_yellow(f_pm25, f_pm10, f_co2, f_tvoc, f_ch2o) ? 1 : 0) : 2;

    if (desired != s_airLevel) {
        if (s_pending != desired) { s_pending = desired; s_pendingSince = now; }
        else if (now - s_pendingSince >= 300000UL) { s_airLevel = desired; s_pending = -1; }
    } else {
        s_pending = -1;
    }

    uint32_t color = (s_airLevel == 2) ? 0xE53935
                   : (s_airLevel == 1) ? 0xFFD600
                   : 0x00C853;
    fill_rgb(color, base);
}

// --- Efekty ---------------------------------------------------------------------
static uint32_t fx_interval(int sp) {
    int ms = 250 - (sp - 1) * 25;   // predkosc 1..10 -> 250..30 ms
    if (ms < 30)  ms = 30;
    if (ms > 250) ms = 250;
    return (uint32_t)ms;
}

static void rainbow_frame(uint8_t base, int sp) {
    uint32_t now = millis();
    if (now - s_lastFx < fx_interval(sp)) return;
    s_lastFx = now;
    fill_rainbow(leds, s_ledCount, s_hue, 7);
    s_hue += 2;
    FastLED.setBrightness(base);
    led_show();
}

static void breathe_frame(uint8_t base, int sp) {
    uint32_t now = millis();
    if (now - s_lastFx < 40) return;   // plynna animacja
    s_lastFx = now;
    static uint16_t phase = 0;
    phase += sp;
    int f = phase & 511;
    if (f > 255) f = 511 - f;
    uint8_t b = (uint8_t)((f / 255.0f) * base);
    if (b == 0) b = 1;
    fill_rgb(current_air_color(), b);
}

static void comet_frame(uint8_t base, int sp) {
    uint32_t now = millis();
    if (now - s_lastFx < fx_interval(sp)) return;
    s_lastFx = now;
    fadeToBlackBy(leds, s_ledCount, 40);
    int head = s_cometPos % s_ledCount;
    if (head < 0) head += s_ledCount;
    leds[head] = CHSV(s_hue, 255, 255);
    s_hue += 4;
    s_cometPos += sp;
    FastLED.setBrightness(base);
    led_show();
}

static void sparkle_frame(uint8_t base, int sp) {
    uint32_t now = millis();
    if (now - s_lastFx < fx_interval(sp)) return;
    s_lastFx = now;
    fadeToBlackBy(leds, s_ledCount, 60);
    int n = 1 + sp / 3;
    for (int i = 0; i < n; i++) leds[random(s_ledCount)] = CHSV(random8(), 255, 255);
    FastLED.setBrightness(base);
    led_show();
}

static void draw_glyph_at(const uint8_t* g, int gx, int gy, CRGB color) {
    for (int c = 0; c < 3; c++) {
        int px = gx + c;
        if (px < 0 || px >= MATRIX_W) continue;
        uint8_t col = pgm_read_byte(&g[c]);
        for (int r = 0; r < 5; r++) {
            if (col & (1 << r)) {
                int py = gy + r;
                if (py < 0 || py >= MATRIX_H) continue;
                int idx = led_idx(px, py);
                if (idx >= 0 && idx < s_ledCount) leds[idx] = color;
            }
        }
    }
}

static void draw_glyph(const uint8_t* g, int gx, CRGB color) {
    draw_glyph_at(g, gx, 1, color);   // wiersze 1..5 (wycentrowane)
}

// Etykieta 3x3 w gornych wierszach 0-2. Krotka = statyczna, dluzsza = przewijana.
static void draw_label3(const char* s, CRGB color) {
    int w = 0;
    for (const char* p = s; *p; p++) w += 4;
    int x;
    if (w <= MATRIX_W) {
        x = 0;                       // mieści sie - statycznie od lewej
    } else {
        x = s_labelX;                // przewijanie
        s_labelX--;
        if (s_labelX < -w) s_labelX = MATRIX_W;
    }
    for (const char* p = s; *p; p++) {
        char ch = *p;
        if (ch >= 'a' && ch <= 'z') ch -= ('a' - 'A');
        const uint8_t* g = font3_glyph(ch);
        for (int col = 0; col < 3; col++) {
            int px = x + col;
            if (px < 0 || px >= MATRIX_W) continue;
            uint8_t b = pgm_read_byte(&g[col]);
            for (int r = 0; r < 3; r++) {
                if (b & (1 << r)) {
                    int idx = led_idx(px, r);   // wiersze 0-2
                    if (idx >= 0 && idx < s_ledCount) leds[idx] = color;
                }
            }
        }
        x += 4;
    }
}

// Kolor wartosci wg konfiguracji (0=auto wg jakosci powietrza, inaczej staly).
static CRGB led_value_color() {
    switch (cfgLedValueColor) {
        case 1:  return CRGB(255, 255, 255);
        case 2:  return CRGB(255, 0, 0);
        case 3:  return CRGB(0, 255, 0);
        case 4:  return CRGB(0, 0, 255);
        case 5:  return CRGB(255, 255, 0);
        case 6:  return CRGB(255, 145, 0);
        case 7:  return CRGB(138, 36, 170);
        default: {
            uint32_t c = current_air_color();
            return CRGB((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
        }
    }
}

// Wartosc (3x5 w wierszach 3-7 albo 4x6 na cala wysokosc). 3 cyfry - przewijane.
static void draw_value(int v, CRGB color) {
    if (v < 0) {
        for (int x = 1; x < 7; x++) {
            int idx = led_idx(x, 3);
            if (idx >= 0 && idx < s_ledCount) leds[idx] = color;
        }
        return;
    }
    if (cfgLedFont == 1) {
        // duza czcionka 4x6 (bez etykiety)
        if (v <= 99) {
            char b[3];
            snprintf(b, sizeof(b), "%02d", v);
            draw_big_glyph(b[0], 0, color);
            draw_big_glyph(b[1], 4, color);
        } else {
            char b[5];
            snprintf(b, sizeof(b), "%d", v);
            int w = 0;
            for (char* p = b; *p; p++) w += 5;
            int x = s_scrollX;
            for (char* p = b; *p; p++) { draw_big_glyph(*p, x, color); x += 5; }
            s_scrollX--;
            if (s_scrollX < -w) s_scrollX = MATRIX_W;
        }
        return;
    }
    // mala czcionka 3x5 (wiersze 1-5)
    if (v <= 99) {
        char b[3];
        snprintf(b, sizeof(b), "%02d", v);
        draw_glyph_at(font_glyph(b[0]), 1, 1, color);
        draw_glyph_at(font_glyph(b[1]), 4, 1, color);
    } else {
        char b[5];
        snprintf(b, sizeof(b), "%d", v);
        int w = 0;
        for (char* p = b; *p; p++) w += 4;
        int x = s_scrollX;
        for (char* p = b; *p; p++) { draw_glyph_at(font_glyph(*p), x, 1, color); x += 4; }
        s_scrollX--;
        if (s_scrollX < -w) s_scrollX = MATRIX_W;
    }
}

// Duza cyfra 4x6 (2 cyfry = 8 kolumn).
static void draw_big_glyph(char ch, int gx, CRGB color) {
    if (ch < '0' || ch > '9') return;
    const uint8_t* g = FONT_4x6[ch - '0'];
    for (int c = 0; c < 4; c++) {
        int px = gx + c;
        if (px < 0 || px >= MATRIX_W) continue;
        uint8_t col = pgm_read_byte(&g[c]);
        for (int r = 0; r < 6; r++) {
            if (col & (1 << r)) {
                int py = 1 + r;   // wiersze 1..6
                int idx = led_idx(px, py);
                if (idx >= 0 && idx < s_ledCount) leds[idx] = color;
            }
        }
    }
}

// Male cyfry 3x5 (2 cyfry, 1 px odstepu, marginesy) - czytelne i nie zajmuja calej matrycy.
static void draw_big_digits(int v, CRGB color) {
    if (v < 0) {
        // brak danych - pozioma kreska
        for (int x = 1; x < 7; x++) {
            int idx = led_idx(x, 3);
            if (idx >= 0 && idx < s_ledCount) leds[idx] = color;
        }
        return;
    }
    if (v > 99) v = 99;
    char b[3];
    snprintf(b, sizeof(b), "%02d", v);
    draw_glyph(font_glyph(b[0]), 1, color);
    draw_glyph(font_glyph(b[1]), 5, color);
}

static int text_width(const char* s) {
    int w = 0;
    for (const char* p = s; *p; p++) w += 4;   // znak 3 px + 1 px odstepu
    return w;
}

static String scroll_text() {
    if (cfgLedText.length()) return cfgLedText;
    struct tm t;
    if (getLocalTime(&t, 100)) {
        char b[8];
        snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min);
        return String(b);
    }
    Channel* pm = channel_find("pm25");
    char b[16];
    snprintf(b, sizeof(b), "PM2.5 %.0f", (pm && pm->valid) ? pm->value : 0.0f);
    return String(b);
}

static void scroll_frame(uint8_t base, int sp) {
    uint32_t now = millis();
    if (now - s_lastFx < fx_interval(sp)) return;
    s_lastFx = now;

    String txt = scroll_text();
    int w = text_width(txt.c_str());

    led_clear();
    int x = s_scrollX;
    for (unsigned i = 0; i < txt.length(); i++) {
        char c = txt[i];
        if (c >= 'a' && c <= 'z') c -= ('a' - 'A');
        draw_glyph(font_glyph(c), x, CRGB::White);
        x += 4;
    }

    s_scrollX--;
    if (s_scrollX < -w) s_scrollX = MATRIX_W;

    FastLED.setBrightness(base);
    led_show();
}

// --- Tryb: pasek poziomu jakosci powietrza (wypelnienie od dolu wg AQI) ---------
static void level_frame(uint8_t base, int sp) {
    (void)sp;
    uint32_t now = millis();
    if (now - s_lastFx < 500) return;   // odswiezanie co 0.5 s
    s_lastFx = now;

    Channel* aq = channel_find("aqi");
    int score = (aq && aq->valid) ? (int)aq->value : -1;

    led_clear();
    if (score > 0) {
        uint32_t color = aqi_color(score > 500 ? 500 : score);
        CRGB c((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
        int rows = (score * MATRIX_H + 250) / 500;   // 0..8 wierszy
        if (rows > MATRIX_H) rows = MATRIX_H;
        for (int y = 0; y < rows; y++) {
            for (int x = 0; x < MATRIX_W; x++) {
                int idx = led_idx(x, MATRIX_H - 1 - y);   // od dolu
                if (idx >= 0 && idx < s_ledCount) leds[idx] = c;
            }
        }
    }
    FastLED.setBrightness(base);
    led_show();
}

// --- Tryb: liczba PM2.5 (2 cyfry, kolor wg jakosci powietrza) ------------------
// Etykieta 3x5 (wiersze 1-5), przewijana gdy szersza niz 8 px.
static void draw_label5(const char* s, CRGB color) {
    int w = 0;
    for (const char* p = s; *p; p++) w += 4;
    int x;
    if (w <= MATRIX_W) x = 0;
    else {
        x = s_labelX;
        s_labelX--;
        if (s_labelX < -w) s_labelX = MATRIX_W;
    }
    for (const char* p = s; *p; p++) {
        char ch = *p;
        if (ch >= 'a' && ch <= 'z') ch -= ('a' - 'A');
        draw_glyph_at(font_glyph(ch), x, 1, color);
        x += 4;
    }
}

// Wartosc 3x5 (wiersze 1-5), 2 cyfry statyczne, 3 cyfry przewijane.
static void draw_value5(int v, CRGB color) {
    if (v < 0) {
        for (int x = 1; x < 7; x++) {
            int idx = led_idx(x, 3);
            if (idx >= 0 && idx < s_ledCount) leds[idx] = color;
        }
        return;
    }
    if (v <= 99) {
        char b[3];
        snprintf(b, sizeof(b), "%02d", v);
        draw_glyph_at(font_glyph(b[0]), 1, 1, color);
        draw_glyph_at(font_glyph(b[1]), 4, 1, color);
    } else {
        char b[5];
        snprintf(b, sizeof(b), "%d", v);
        int w = 0;
        for (char* p = b; *p; p++) w += 4;
        int x = s_scrollX;
        for (char* p = b; *p; p++) { draw_glyph_at(font_glyph(*p), x, 1, color); x += 4; }
        s_scrollX--;
        if (s_scrollX < -w) s_scrollX = MATRIX_W;
    }
}

static void num_frame(uint8_t base, int sp) {
    uint32_t now = millis();
    Channel* pm = channel_find("pm25");
    int v = (pm && pm->valid) ? (int)(pm->value + 0.5f) : -1;

    if (cfgLedFont == 1) {
        uint32_t iv = (v >= 100) ? fx_interval(sp) : 500;
        if (now - s_lastFx < iv) return;
        s_lastFx = now;
        led_clear();
        draw_value(v, led_value_color());   // duza 4x6
        FastLED.setBrightness(base);
        led_show();
        return;
    }

    // font=0: etykieta i wartosc naprzemiennie co 2 s
    bool showLabel = ((now / 2000) % 2) == 0;
    uint32_t iv = (!showLabel && v >= 100) ? fx_interval(sp) : 500;
    if (now - s_lastFx < iv) return;
    s_lastFx = now;
    led_clear();
    if (showLabel) draw_label5("PM", CRGB::White);
    else           draw_value5(v, led_value_color());
    FastLED.setBrightness(base);
    led_show();
}

// --- Tryb: buzka (emoji 8x8) wg jakosci powietrza, z mruganiem -----------------
static void smiley_frame(uint8_t base, int sp) {
    (void)sp;
    Channel* aq = channel_find("aqi");
    int score = (aq && aq->valid) ? (int)aq->value : -1;
    uint32_t now = millis();

    const uint8_t* face;
    uint32_t color;
    if (score < 0)            { face = FACE_SAD;     color = 0x8E24AA; }
    else if (score <= 50)     { face = FACE_HAPPY;   color = 0x00C853; }
    else if (score <= 100)    { face = FACE_NEUTRAL; color = 0xFFD600; }
    else if (score <= 150)    { face = FACE_SAD;     color = 0xFF9100; }
    else                      { face = FACE_SICK;    color = 0xE53935; }

    // Mruganie: zamknij oczy na 200 ms co ~3 s (tylko gdy sa dane).
    if (score >= 0 && (now % 3000) < 200) face = FACE_BLINK;

    if (now - s_lastFx < 200) return;   // odswiezanie co 0.2 s (mruganie)
    s_lastFx = now;

    CRGB c((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
    led_clear();
    for (int y = 0; y < MATRIX_H; y++) {
        uint8_t row = pgm_read_byte(&face[y]);
        for (int x = 0; x < MATRIX_W; x++) {
            if (row & (1 << x)) {
                int idx = led_idx(x, y);
                if (idx >= 0 && idx < s_ledCount) leds[idx] = c;
            }
        }
    }
    FastLED.setBrightness(base);
    led_show();
}

// --- Tryb: cyfry AQI (2 cyfry statyczne, powyzej 99 - 3 cyfry przewijane) -------
static void aqi_frame(uint8_t base, int sp) {
    Channel* aq = channel_find("aqi");
    int score = (aq && aq->valid) ? (int)aq->value : -1;
    uint32_t now = millis();

    if (cfgLedFont == 1) {
        uint32_t iv = (score >= 100) ? fx_interval(sp) : 500;
        if (now - s_lastFx < iv) return;
        s_lastFx = now;
        led_clear();
        draw_value(score, led_value_color());   // duza 4x6
        FastLED.setBrightness(base);
        led_show();
        return;
    }

    // font=0: etykieta i wartosc naprzemiennie co 2 s
    bool showLabel = ((now / 2000) % 2) == 0;
    uint32_t iv = (!showLabel && score >= 100) ? fx_interval(sp) : 500;
    if (now - s_lastFx < iv) return;
    s_lastFx = now;
    led_clear();
    if (showLabel) draw_label5("IAQ", CRGB::White);
    else           draw_value5(score, led_value_color());
    FastLED.setBrightness(base);
    led_show();
}

// --- Pomocnicze: rysowanie bitmap i cyfr ---------------------------------------
static void draw_bitmap(const uint8_t* bmp, CRGB c) {
    for (int y = 0; y < MATRIX_H; y++) {
        uint8_t row = pgm_read_byte(&bmp[y]);
        for (int x = 0; x < MATRIX_W; x++) {
            if (row & (1 << x)) {
                int idx = led_idx(x, y);
                if (idx >= 0 && idx < s_ledCount) leds[idx] = c;
            }
        }
    }
}

static void draw_2digits(int v, CRGB c) {
    draw_big_digits(v, c);
}

// --- Tryb: Matrix Rain ---------------------------------------------------------
static void matrix_frame(uint8_t base, int sp) {
    uint32_t now = millis();
    if (now - s_lastFx < fx_interval(sp)) return;
    s_lastFx = now;
    for (int y = MATRIX_H - 1; y >= 1; y--)
        for (int x = 0; x < MATRIX_W; x++)
            leds[led_idx(x, y)] = leds[led_idx(x, y - 1)];
    for (int x = 0; x < MATRIX_W; x++) {
        int idx = led_idx(x, 0);
        leds[idx] = (random(100) < 15) ? CRGB(0, 255, 0) : CRGB(0, 0, 0);
    }
    FastLED.setBrightness(base);
    led_show();
}

// --- Tryb: Ogien -----------------------------------------------------------------
static uint8_t s_heat[MATRIX_N];

static void fire_frame(uint8_t base, int sp) {
    (void)sp;
    uint32_t now = millis();
    if (now - s_lastFx < 50) return;
    s_lastFx = now;

    for (int i = 0; i < MATRIX_N; i++) {
        int c = s_heat[i] - random(0, 6);
        if (c < 0) c = 0;
        s_heat[i] = c;
    }
    for (int x = 0; x < MATRIX_W; x++) s_heat[led_idx(x, MATRIX_H - 1)] = random(120, 256);
    for (int y = 0; y < MATRIX_H - 1; y++) {
        for (int x = 0; x < MATRIX_W; x++) {
            int idx = led_idx(x, y);
            int b1 = led_idx(x, y + 1);
            int b2 = led_idx(x > 0 ? x - 1 : x, y + 1);
            int b3 = led_idx(x < MATRIX_W - 1 ? x + 1 : x, y + 1);
            s_heat[idx] = (s_heat[b1] + s_heat[b2] + s_heat[b3]) / 3;
        }
    }
    for (int i = 0; i < MATRIX_N; i++) {
        uint8_t h = s_heat[i];
        if (h < 85)       leds[i] = CRGB(0, 0, 0);
        else if (h < 140) leds[i] = CRGB(h * 3, 0, 0);
        else if (h < 180) leds[i] = CRGB(255, (h - 140) * 6, 0);
        else              leds[i] = CRGB(255, 255, (h - 180) * 3);
    }
    FastLED.setBrightness(base);
    led_show();
}

// --- Tryb: Plasma ----------------------------------------------------------------
static void plasma_frame(uint8_t base, int sp) {
    uint32_t now = millis();
    if (now - s_lastFx < 50) return;
    s_lastFx = now;
    static uint8_t t = 0;
    t += sp;
    for (int y = 0; y < MATRIX_H; y++) {
        for (int x = 0; x < MATRIX_W; x++) {
            int v = (sin8(x * 32 + t) + sin8(y * 48 - t) + sin8((x + y) * 24 + t * 2)) / 3;
            leds[led_idx(x, y)] = CHSV(v + 96, 255, 255);
        }
    }
    FastLED.setBrightness(base);
    led_show();
}

// --- Tryb: Bouncing ball ----------------------------------------------------------
static void bounce_frame(uint8_t base, int sp) {
    uint32_t now = millis();
    if (now - s_lastFx < fx_interval(sp)) return;
    s_lastFx = now;
    static int bx = 2, by = 2, dx = 1, dy = 1;
    fadeToBlackBy(leds, s_ledCount, 32);
    leds[led_idx(bx, by)] = CHSV(s_hue, 255, 255);
    bx += dx; by += dy;
    if (bx <= 0 || bx >= MATRIX_W - 1) { dx = -dx; s_hue += 32; }
    if (by <= 0 || by >= MATRIX_H - 1) { dy = -dy; s_hue += 32; }
    FastLED.setBrightness(base);
    led_show();
}

// --- Tryb: Ikonki (slajd pixel-art) ------------------------------------------------
static int s_iconIdx = 0;

static void icons_frame(uint8_t base, int sp) {
    (void)sp;
    uint32_t now = millis();
    if (now - s_lastFx >= 1500) {
        s_lastFx = now;
        s_iconIdx = (s_iconIdx + 1) % ICON_COUNT;
        s_hue += 24;
    }
    led_clear();
    draw_bitmap(ICONS[s_iconIdx], CHSV(s_hue, 255, 255));
    FastLED.setBrightness(base);
    led_show();
}

// --- Tryb: Auto (cykl ekranow z danymi czujnikow) ----------------------------------
static void auto_frame(uint8_t base, int sp) {
    uint32_t now = millis();
    static int sub = 0;
    static uint32_t s_autoNext = 0;
    if (now - s_autoNext >= 2500) { s_autoNext = now; sub = (sub + 1) % 8; s_scrollX = MATRIX_W; }

    Channel* aq = channel_find("aqi");
    Channel* pm = channel_find("pm25");
    Channel* tt = channel_find("temp");
    Channel* hh = channel_find("hum");
    uint32_t color = current_air_color();
    CRGB cc((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);

    led_clear();

    if (sub == 7) {
        // Zegar - przewijany.
        if (now - s_lastFx >= fx_interval(sp)) {
            s_lastFx = now;
            struct tm t;
            if (getLocalTime(&t, 100)) {
                char b[8];
                snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min);
                int w = text_width(b);
                int x = s_scrollX;
                for (char* p = b; *p; p++) { draw_glyph(font_glyph(*p), x, CRGB::White); x += 4; }
                s_scrollX--;
                if (s_scrollX < -w) s_scrollX = MATRIX_W;
            } else {
                draw_2digits(-1, cc);
            }
        }
    } else {
        switch (sub) {
            case 0: {
                int s = (aq && aq->valid) ? (int)aq->value : -1;
                const uint8_t* f = FACE_HAPPY;
                if (s < 0)        f = FACE_SAD;
                else if (s <= 50) f = FACE_HAPPY;
                else if (s <= 100) f = FACE_NEUTRAL;
                else if (s <= 150) f = FACE_SAD;
                else               f = FACE_SICK;
                draw_bitmap(f, cc);
                break;
            }
            case 1: draw_bitmap(ICONS[ICON_CLOUD], cc); break;
            case 2: draw_2digits(pm && pm->valid ? (int)(pm->value + 0.5f) : -1, cc); break;
            case 3: draw_bitmap(ICONS[ICON_TERM], cc); break;
            case 4: draw_2digits(tt && tt->valid ? (int)(tt->value + 0.5f) : -1, cc); break;
            case 5: draw_bitmap(ICONS[ICON_DROP], cc); break;
            case 6: draw_2digits(hh && hh->valid ? (int)(hh->value + 0.5f) : -1, cc); break;
        }
    }
    FastLED.setBrightness(base);
    led_show();
}

void led_loop() {
    if (!s_initialized || s_ledCount == 0) return;
    uint32_t now = millis();

    // Diagnostyka: 4 diody (indeksy lancucha) - podtrzymaj do wygasniecia.
    if (now < s_dotsUntil) return;

    // Podglad z www ma pierwszenstwo przez krotki czas.
    if (now < s_testUntil) {
        fill_rgb(s_testColor, led_eff_brightness());
        return;
    }

    // Alarm (gaz / awaria czujnikow) - czerwony flash ZAWSZE, niezaleznie od trybu.
    if (air_alarm_active()) {
        uint8_t base = is_night() ? led_eff_brightness() / 2 : led_eff_brightness();
        bool on = ((now / 500) % 2) == 0;
        fill_rgb(0xFF0000, on ? base : 0);
        return;
    }

    uint8_t base = is_night() ? led_eff_brightness() / 2 : led_eff_brightness();
    int sp = s_speed >= 1 ? s_speed : 5;

    switch (s_mode) {
        case LED_MODE_AIR:     air_quality_frame(now); break;
        case LED_MODE_RAINBOW: rainbow_frame(base, sp); break;
        case LED_MODE_BREATHE: breathe_frame(base, sp); break;
        case LED_MODE_COMET:   comet_frame(base, sp); break;
        case LED_MODE_SPARKLE: sparkle_frame(base, sp); break;
        case LED_MODE_SCROLL:  scroll_frame(base, sp); break;
        case LED_MODE_LEVEL:   level_frame(base, sp); break;
        case LED_MODE_NUM:     num_frame(base, sp); break;
        case LED_MODE_SMILEY:  smiley_frame(base, sp); break;
        case LED_MODE_AQI:     aqi_frame(base, sp); break;
        case LED_MODE_MATRIX:  matrix_frame(base, sp); break;
        case LED_MODE_FIRE:    fire_frame(base, sp); break;
        case LED_MODE_PLASMA:  plasma_frame(base, sp); break;
        case LED_MODE_BOUNCE:  bounce_frame(base, sp); break;
        case LED_MODE_AUTO:    auto_frame(base, sp); break;
        case LED_MODE_ICONS:   icons_frame(base, sp); break;
        default:               air_quality_frame(now); break;
    }
}

void led_test(uint32_t color, uint16_t ms) {
    s_testColor = color;
    s_testUntil = millis() + ms;
}

// Diagnostyka: zaswiec 4 diody o znanych indeksach lancucha:
//   [0]=czerwona, [7]=zielona, [8]=niebieska, [63]=biala.
// Pozwala ustalic uklad (serpentine/progressive) i orientacje matrycy.
void led_test_dots(uint16_t ms) {
    if (!s_initialized || s_ledCount == 0) return;
    memset(leds, 0, s_ledCount * sizeof(CRGB));
    if (s_ledCount > 0)  leds[0]  = CRGB(255, 0, 0);
    if (s_ledCount > 7)  leds[7]  = CRGB(0, 255, 0);
    if (s_ledCount > 8)  leds[8]  = CRGB(0, 0, 255);
    if (s_ledCount > 63) leds[63] = CRGB(255, 255, 255);
    FastLED.setBrightness(led_eff_brightness());
    led_show();
    s_dotsUntil = millis() + ms;
    LOG_I("LED: diagnostyka 4 diod (%u ms)", ms);
}

void led_set_mode(int mode, int speed, bool save) {
    if (mode < 0 || mode >= LED_MODE_COUNT) mode = 0;
    if (speed < 1 || speed > 10) speed = 5;
    s_mode  = mode;
    s_speed = speed;
    s_scrollX = MATRIX_W;
    cfgLedMode  = mode;
    cfgLedSpeed = speed;
    if (save) nvs_save();
    LOG_I("LED: tryb %s (%d), predkosc %d%s",
          led_mode_name(mode), mode, speed, save ? " - zapisano" : "");
}

int led_current_mode()  { return s_mode; }
int led_current_speed() { return s_speed; }

void led_set_order(int order, bool save) {
    if (order < 0 || order > 5) order = 0;
    cfgLedOrder = order;
    if (save) nvs_save();
    led_show();   // od razu pokaz z nowa kolejnoscia
    LOG_I("LED: kolejnosci kolorow %d%s", order, save ? " - zapisano" : "");
}

void led_set_brightness(int b, bool save) {
    if (b < 1)   b = 1;
    if (b > 255) b = 255;
    cfgLedBrightness = b;
    if (save) nvs_save();
    FastLED.setBrightness(led_eff_brightness());
    led_show();
    LOG_I("LED: jasnosc %d (efekt. %d)%s", b, led_eff_brightness(), save ? " - zapisano" : "");
}

const char* led_mode_name(int mode) {
    switch (mode) {
        case LED_MODE_AIR:     return "powietrze";
        case LED_MODE_RAINBOW: return "tecza";
        case LED_MODE_BREATHE: return "oddychanie";
        case LED_MODE_COMET:   return "kometa";
        case LED_MODE_SPARKLE: return "iskry";
        case LED_MODE_SCROLL:  return "tekst";
        case LED_MODE_LEVEL:   return "poziom";
        case LED_MODE_NUM:     return "liczba";
        case LED_MODE_SMILEY:  return "buzka";
        case LED_MODE_AQI:     return "aqi";
        case LED_MODE_MATRIX:  return "matrix";
        case LED_MODE_FIRE:    return "ogien";
        case LED_MODE_PLASMA:  return "plasma";
        case LED_MODE_BOUNCE:  return "pilka";
        case LED_MODE_AUTO:    return "auto";
        case LED_MODE_ICONS:   return "ikony";
        default:               return "?";
    }
}

int led_mode_from_name(const char* s) {
    if (!s) return -1;
    if (!strcmp(s, "air")       || !strcmp(s, "powietrze"))  return LED_MODE_AIR;
    if (!strcmp(s, "rainbow")   || !strcmp(s, "tecza"))      return LED_MODE_RAINBOW;
    if (!strcmp(s, "breathe")   || !strcmp(s, "oddychanie")) return LED_MODE_BREATHE;
    if (!strcmp(s, "comet")     || !strcmp(s, "kometa"))     return LED_MODE_COMET;
    if (!strcmp(s, "sparkle")   || !strcmp(s, "iskry"))      return LED_MODE_SPARKLE;
    if (!strcmp(s, "scroll")    || !strcmp(s, "tekst"))      return LED_MODE_SCROLL;
    if (!strcmp(s, "level")     || !strcmp(s, "poziom"))     return LED_MODE_LEVEL;
    if (!strcmp(s, "num")       || !strcmp(s, "liczba"))     return LED_MODE_NUM;
    if (!strcmp(s, "smiley")    || !strcmp(s, "buzka") || !strcmp(s, "buźka")) return LED_MODE_SMILEY;
    if (!strcmp(s, "aqi")       || !strcmp(s, "cyfry"))      return LED_MODE_AQI;
    if (!strcmp(s, "matrix")    || !strcmp(s, "deszcz"))     return LED_MODE_MATRIX;
    if (!strcmp(s, "fire")      || !strcmp(s, "ogien"))      return LED_MODE_FIRE;
    if (!strcmp(s, "plasma"))                               return LED_MODE_PLASMA;
    if (!strcmp(s, "bounce")    || !strcmp(s, "pilka"))      return LED_MODE_BOUNCE;
    if (!strcmp(s, "auto"))                                 return LED_MODE_AUTO;
    if (!strcmp(s, "icons")     || !strcmp(s, "ikony"))      return LED_MODE_ICONS;
    return -1;
}
