#pragma once
// Karta microSD (SPI) - zapis pomiarow do CSV (/logs/filament-YYYY-MM.csv).
#include <Arduino.h>

void sd_begin();
void sd_loop();
bool sd_ready();
