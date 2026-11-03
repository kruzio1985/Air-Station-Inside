#pragma once
// =============================================================================
//  FilamentMonitor - stale i wartosci domyslne (kompilacja)
// =============================================================================

// --- Tozsamosc urzadzenia ----------------------------------------------------
#define FW_NAME             "FilamentMonitor"
#define FW_VERSION          "1.0.0"
#define FW_HOSTNAME         "filament-monitor"
#define DEVICE_NAME         "Stacja Powietrza"   // nazwa urzadzenia w Home Assistant

// --- Wi-Fi / AP awaryjny ------------------------------------------------------
#define AP_SSID_DEFAULT     "FilamentMonitor"
#define AP_PASS_DEFAULT     "changeme"
#define WIFI_CONNECT_MS     15000    // czas prob polaczenia z WiFi przed AP

// --- MQTT / Home Assistant ----------------------------------------------------
#define MQTT_PORT_DEFAULT   1883
#define MQTT_PREFIX_DEFAULT "stacja_powietrza"

// --- Zewnetrzny czujnik temp/wilgotnosci z Home Assistant (REST) ----------------
#define HA_POLL_MS          60000    // odczyt zewnetrznego czujnika HA co 60 s

// --- Korekcja wilgotnosci PM (czujnik optyczny zawyza przy wysokim RH) ---------
// PM_corr = PM_raw / (1 + coef * (RH - prog) / (100 - prog)), dla RH > prog.
#define PM_RH_THRESH_DEFAULT  60.0f
#define PM_RH_COEF_DEFAULT    0.5f
#define MQTT_INTERVAL_MS    5000     // publikacja stanu co 5 s

// --- Wezel pojemnikow (ESP32-C6 + 16x AHT20, MQTT) ---------------------------
// Tematy wezla:  <Z2M_TOPIC>/box/<N>/temp|hum|meta   (N = 1..BOX_COUNT)
#define Z2M_TOPIC_DEFAULT   "filamentnode"
#define BOX_COUNT           16

// --- Pasek LED / matryca LED 8x8 (64 diody WS2812B) -----------------------------
#define LED_COUNT_DEFAULT   64      // matryca 8x8; przy pasku ustaw na stronie
#define LED_BRIGHT_DEFAULT  127     // 50% - domyslna jasnosc
#define LED_BRIGHT_MAX      127     // twardy limit 50% jasnosci (0..255)
#define LED_MAX_MA_DEFAULT  2000    // limit pradu (mA) - pozwala osiagnac 50% na 64 diodach

// --- Progi jakosci powietrza (kolory paska) ------------------------------------
// AQI US EPA (z PM2.5):
//   <=50 zielony, 51-100 zolty, 101-150 pomaranczowy, >150 czerwony
#define AQI_GOOD            50
#define AQI_MODERATE        100
#define AQI_UNHEALTHY       150

// --- Progi wilgotnosci filamentu (domyslne, edytowalne na stronie) -------------
// % RH - material w pojemniku decyduje (PLA/PETG < 30, Nylon/PC/PVA < 15).
#define BOX_RH_WARN         30.0f
#define BOX_RH_ALARM        40.0f

// --- Karta SD ------------------------------------------------------------------
#define SD_LOG_DIR          "/logs"
#define SD_LOG_INTERVAL_MS  300000   // zapis paczki rekordow minutowych co 5 min
#define SD_CSV_FILENAME     "stacja_powietrza" // .../YYYY-MM.csv (format: rekordy minutowe)

// --- Czujniki -------------------------------------------------------------------
#define PMS_UART_BAUD       9600
#define TVOC_UART_BAUD      115200
#define ADS1115_I2C_ADDR    0x48

// --- MiCS5524 (czujnik CO/alkoholi, MEMS, zasilanie 5 V) ----------------------
// Modul 4-pin: VCC(5V) GND A0(wyjscie analogowe) EN(wlaczanie czujnika).
// EN jest AKTYWNY NISKI: LOW/GND = grzalka ON, HIGH = grzalka OFF.
// ADS1115 zasilany z 5 V -> wejscie analogowe 0-5 V, wiec A0 idzie WPROST
// do AIN0 (bez dzielnika). Uwaga o I2C: patrz docs/LUTOWANIE.md.
#define MICS_VCC            5.0f     // napiecie zasilania czujnika (szyna 5 V)
#define MICS_R_DIVIDER      1.0f     // 1.0 = bez dzielnika (ADS1115 na 5 V)
#define MICS_ADC_CHANNEL    0        // AIN0
// Czas wygrzewania grzalki do stabilnych odczytow (15 min).
#define MICS_WARMUP_MS      (15UL*60UL*1000UL)
// Szacunkowa krzywa CO (datasheet: Rs/R0=0.5 @100ppm, 0.2 @1000ppm):
//   ratio = A * C^-B  ->  C = (A/ratio)^(1/B).  A=3.125, 1/B=2.5126.
// Uwaga: czujnik NIESELEKTYWNY - te same wartosci daje etanol/H2/NH3.
#define MICS_CO_A           3.125f
#define MICS_CO_B           2.5126f

// --- MQ-7 (czujnik CO, modul analogowy, A0 -> ADS1115 AIN2) ---------------------
// Modul 4-pin: VCC(5V) GND A0(wyjscie analogowe) D0(wyjscie cyfrowe, nieuzywane).
// UWAGA: MQ-7 do DOKLADNEGO pomiaru CO wymaga cyklu grzalki 5V/1.4V; proste
// moduly grzeja stale 5V, wiec wynik traktuj jako orientacyjny. Przed kalibracja
// R0 wygrzewaj czujnik min. 24-48 h w czystym powietrzu.
#define MQ7_ADC_CHANNEL     2        // AIN2 na ADS1115 (AIN0=MiCS, AIN1 wolny)
#define MQ7_WARMUP_MS       (15UL*60UL*1000UL)   // ukryj CO przez pierwsze 15 min po starcie
// Krzywa CO MQ-7 (przyblizona): ppm = A * (Rs/R0)^(-B).
// Dopasowane z datasheetu: 10 ppm ~ Rs/R0=26, 100 ppm ~ Rs/R0=1.
#define MQ7_CO_A            100.0f
#define MQ7_CO_B            0.7067f

// --- OTA -------------------------------------------------------------------------
#define OTA_PASSWORD        "changeme"
