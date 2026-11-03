#pragma once
// =============================================================================
//  FilamentMonitor - wspolny stan aplikacji (kanaly, pojemniki, ustawienia)
// =============================================================================
#include <Arduino.h>
#include "config.h"
#include "nvs_store.h"

// --- Kanal pomiarowy ---------------------------------------------------------
struct Channel {
    const char* id;      // stabilny identyfikator (topic MQTT / kolumna CSV)
    const char* name;    // nazwa wyswietlana
    const char* unit;    // jednostka
    float value;         // ostatnia wartosc
    float min;           // zakres min (do wykresow / sanity)
    float max;           // zakres max
    bool valid;          // false = brak odczytu
    uint32_t lastMs;     // czas ostatniego pomiaru
};

// --- Pojemnik na filament (dane z wezla Zigbee, etap 2) ----------------------
struct Box {
    float temp;          // C
    float hum;           // % RH
    bool  online;        // czy dostalismy swieza wartosc
    uint32_t lastMs;
};

// --- Opis pojemnika (metadane edytowane na stronie) ---------------------------
struct BoxMeta {
    char  name[24];      // np. "Pojemnik 1" / "Czarny PLA"
    char  type[16];      // rodzaj filamentu: PLA, PETG, ABS, TPU, Nylon...
    char  color[16];     // kolor
    char  brand[24];     // producent
    float targetRh;      // wilgotnosc docelowa wg karty produktu [% RH]
};

// --- Zmienne globalne --------------------------------------------------------
extern Channel channels[];
extern const int channelCount;
extern Box boxes[BOX_COUNT];
extern BoxMeta boxMeta[BOX_COUNT];

// --- Kanaly dostepne w tym projekcie ------------------------------------------
// Indeksy kanalow (zgodne z tablica channels[]):
enum ChanIdx {
    CH_PM1_CF1 = 0,     // PM1.0 [ug/m3]
    CH_PM25_CF1,        // PM2.5 [ug/m3]
    CH_PM10_CF1,        // PM10  [ug/m3]
    CH_AQI,             // IAQ (najgorszy z PM2.5/PM10/CO2/TVOC/CH2O), 0-500
    CH_CO2,             // CO2 [ppm] (TVOC 30712)
    CH_CH2O,            // formaldehyd [ppb]
    CH_TVOC,            // TVOC [ppm]
    CH_TVOC_LEVEL,      // poziom 1..4 (TVOC 30712)
    CH_MICS_V,          // MiCS5524 napiecie czujnika [V] (po odtworzeniu z dzielnika)
    CH_MICS_RSR0,       // stosunek Rs/R0 (czyste powietrze ~1.0)
    CH_MICS_CO,         // ekwiwalent CO [ppm] (przyblizony)
    CH_MICS_WARMUP,     // 1 = czujnik sie wygrzewa
    CH_MQ7_V,           // MQ-7 napiecie czujnika [V]
    CH_MQ7_RS,          // MQ-7 stosunek Rs/R0
    CH_MQ7_CO,          // MQ-7 CO [ppm] (orientacyjny)
    CH_MOTION,          // ruch: 0/1
    CH_TEMP,            // temperatura [deg C] (DHT11)
    CH_HUM,             // wilgotnosc [% RH] (DHT11)
    CH_PM25_1M,         // srednia 1-min PM2.5
    CH_PM25_5M,         // srednia 5-min PM2.5
    CH_PM25_24H,        // srednia 24-h PM2.5
    CH_TEMP_24H,        // srednia 24-h temperatura
    CH_HUM_24H,         // srednia 24-h wilgotnosc
    CH_TEMP_EXT,        // temperatura zewnetrzna (czujnik z HA)
    CH_HUM_EXT,         // wilgotnosc zewnetrzna (czujnik z HA)
    CH_RH_CORR,         // RH uzyta do korekcji PM (valid = korekcja aktywna)
};

// --- Pomocnicze ---------------------------------------------------------------
void channels_init();                 // buduje liste kanalow
Channel* channel_find(const char* id);
