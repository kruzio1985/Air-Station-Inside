#pragma once
// =============================================================================
//  FilamentMonitor - ustawienia trwale (NVS przez Preferences)
//  Ustawienia przetrwaja aktualizacje firmware (OTA) i uploadfs.
// =============================================================================
#include <Arduino.h>

// --- Ustawienia urzadzenia (wczytane z NVS, edytowalne na stronie) ------------
extern String cfgWifiSsid;
extern String cfgWifiPass;
extern String cfgHostname;
extern bool   cfgWifiStatic;
extern String cfgWifiIp;
extern String cfgWifiMask;
extern String cfgWifiGw;
extern String cfgWifiDns;

extern String cfgMqttHost;
extern int    cfgMqttPort;
extern String cfgMqttUser;
extern String cfgMqttPass;
extern String cfgMqttPrefix;
extern String cfgZ2mTopic;      // prefix tematow wezla pojemnikow (np. "filamentnode")
extern bool   cfgBoxMetaFromNode; // aktualizuj opisy (typ/kolor/producent) z meta wezla

extern String cfgHaUrl;         // np. http://192.168.1.14:8123
extern String cfgHaToken;       // token dlugoterminowy HA (pusty = wylaczone)
extern String cfgHaTempEntity;  // entity temperatura (czujnik zewnetrzny)
extern String cfgHaHumEntity;   // entity wilgotnosc (czujnik zewnetrzny)

extern bool   cfgAirEnabled;    // publikacja zbiorczego JSON jakosci powietrza
extern String cfgAirTopic;      // temat JSON dla stacji pogodowej

extern int    cfgLedCount;
extern int    cfgLedBrightness; // 0..255
extern int    cfgLedMaxMa;

// --- Matryca LED 8x8 / pasek --------------------------------------------------
extern bool   cfgLedMatrix;     // true = matryca 8x8 (64 diody), false = pasek
extern int    cfgLedMode;       // 0=powietrze, 1=tecza, 2=oddychanie, 3=kometa, 4=iskry, 5=tekst
extern int    cfgLedSpeed;      // predkosc efektu 1..10
extern int    cfgLedLayout;     // 0=serpentine (zygzak), 1=progressive
extern int    cfgLedRotate;     // obrot matrycy 0..3 (0/90/180/270 stopni)
extern bool   cfgLedFlipY;      // odbicie w pionie (matryca zamontowana do gory nogami)
extern int    cfgLedOrder;      // kolejnosc kolorow: 0=GRB,1=RGB,2=BGR,3=BRG,4=GBR,5=RBG
extern int    cfgLedFont;       // wielkosc cyfr: 0=mala (3x5), 1=duza (4x6)
extern int    cfgLedValueColor; // kolor wartosci: 0=auto,1=bialy,2=czerwony,3=zielony,4=niebieski,5=zolty,6=pomaranczowy,7=fioletowy
extern bool   cfgLedAlwaysOn;   // true = swieci bez wzgledu na ruch (PIR)
extern String cfgLedText;       // tekst przewijany w trybie "tekst" (pusty = zegar HH:MM)

// --- Kalibracja ---------------------------------------------------------------
extern float  cfgMicsR0;        // napiecie MiCS5524 w czystym powietrzu [V]
extern float  cfgMq7R0;         // napiecie MQ-7 w czystym powietrzu [V]
extern float  cfgPmOffset;      // offset PM2.5 [ug/m3]

// --- Korekcja wilgotnosci PM ---------------------------------------------------
extern bool   cfgPmRhCorr;      // wlacz korekcje wilgotnosciowa PM
extern float  cfgPmRhThresh;    // prog RH [%], powyzej ktorego korygujemy
extern float  cfgPmRhCoef;      // wspolczynnik korekcji

// --- Progi (edytowalne) --------------------------------------------------------
extern float  cfgBoxRhWarn;     // % RH - ostrzezenie (zolty)
extern float  cfgBoxRhAlarm;    // % RH - alarm (czerwony)

// --- Ruch (PIR) ----------------------------------------------------------------
extern bool   cfgPirEnabled;    // czy lokalny PIR jest aktywny
extern bool   cfgPirInverted;   // true = czujnik aktywny NISKIM (spoczynek HIGH)
extern int    cfgPirHoldMs;     // jak dlugo "ruch" trwa po ostatnim zdarzeniu [ms]
extern int    cfgPirDebounceMs; // minimalny czas stabilnego impulsu, zeby uznac ruch [ms]

// --- Inicjalizacja / zapis -----------------------------------------------------
void nvs_begin();                 // laduje ustawienia (lub zapisuje domyslne)
void nvs_save();                  // zapisuje wszystkie ustawienia do NVS
void nvs_factory_reset();         // czysci NVS i zapisuje domyslne

// --- Kopia zapasowa ustawien (JSON) ---------------------------------------------
// Eksport/import calej konfiguracji - zgodne z zakladka "Konfiguracja".
String nvs_export_json();
bool   nvs_import_json(const String& json);
