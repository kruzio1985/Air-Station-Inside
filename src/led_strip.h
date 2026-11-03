#pragma once
// Pasek LED RGBIC (WS2812B) albo matryca 8x8 - stan jakosci powietrza,
// ruch + alarm oraz efekty (tecza, oddychanie, kometa, iskry, tekst).
#include <Arduino.h>

void led_begin();
void led_loop();
void led_apply_settings();                 // po zmianie ilosci diod / jasnosci
void led_test(uint32_t color, uint16_t ms); // podglad z www (auto-wylacza sie)
void led_test_dots(uint16_t ms);           // diagnostyka: 4 diody w rogach lancucha
uint8_t led_eff_brightness();              // jasnosc po limicie pradu

void        led_set_mode(int mode, int speed, bool save); // tryb efektu (save = do NVS)
void        led_set_order(int order, bool save);          // kolejnosci kolorow 0..5 (save = do NVS)
void        led_set_brightness(int b, bool save);         // jasnosc 1..255 (save = do NVS)
int         led_current_mode();
int         led_current_speed();
const char* led_mode_name(int mode);
int         led_mode_from_name(const char* s);            // -1 = nieznany
