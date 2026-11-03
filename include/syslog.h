#pragma once
// =============================================================================
//  FilamentMonitor - prosty logger (konsola + bufor pierscieniowy w PSRAM)
//  Poziomy: I(INFO) W(WARN) E(ERROR). Bufor sluzy zakladce "Logi".
// =============================================================================
#include <Arduino.h>

void  syslog_init();
void  LOG_I(const char* fmt, ...);
void  LOG_W(const char* fmt, ...);
void  LOG_E(const char* fmt, ...);

// Pobiera logi w formie tekstu (nowe linie na koncu). clear=true czysci bufor.
String syslog_dump(bool clear);
size_t syslog_size();
