#pragma once
// =============================================================================
//  FilamentMonitor - mapa pinow (wartosci DOMYSLNE)
//  Edytuj tutaj po zlutowaniu. Zmiany bez rekompilacji: zakladka "Piny"
//  (planowana w pozniejszym etapie; na razie piny sa stale w tym pliku).
//  Plytka: ESP32-S3 WROOM-1 N16R8
// =============================================================================

// --- I2C (magistrala glowna: ADS1115 0x48, opcjonalnie DS3231 0x68) ----------
#define PIN_SDA              8
#define PIN_SCL              9

// --- I2C #2 (SHT31 temp/wilgotnosc; NIE uzywac GPIO48 = wbudowana dioda RGB) --
#define PIN_SDA2             47
#define PIN_SCL2             40

// --- PMS7003 (UART2, 9600) --------------------------------------------------
#define PIN_PMS_RX           18    // PMS7003 TX -> 18
#define PIN_PMS_TX           17    // -> PMS7003 RX (tryb pasywny / komendy)
#define PIN_PMS_SET          -1    // SET (sen). -1 = praca ciagla na 3V3

// --- Waveshare TVOC SKU 30712 (UART1, 115200, ramka FE ... 16 co 1 s) --------
// UWAGA: zgodnie z Twoim lutowaniem TX z Waveshare idzie na GPIO14,
// a RX Waveshare na GPIO15 (odwrotnie niz w pierwszej wersji mapy).
#define PIN_TVOC_RX          14    // TVOC TX -> 14
#define PIN_TVOC_TX          15    // -> TVOC RX (komendy trybu query, opcjonalne)

// --- Karta microSD (SPI) ----------------------------------------------------
// SPI na ESP32-S3 mozna mapowac na dowolne wolne GPIO (macierz GPIO).
// Przeniesione z 10-13, zeby nie przedluzac kabla. GPIO 7 zostaje dla LED.
#define PIN_SD_SCK           4
#define PIN_SD_MOSI          5
#define PIN_SD_MISO          6
#define PIN_SD_CS            2     // bylo 22; GPIO22 nie wyprowadzone na plytce

// --- Pasek LED RGBIC (WS2812B, dane; 5 V z osobnego zasilacza) ---------------
#define PIN_RGB              7

// --- Ruch lokalny (PIR AM312 / LD2410 OUT), przerwanie ----------------------
#define PIN_PIR              21

// --- MiCS5524: EN (wlaczanie czujnika, AKTYWNY NISKI = WLACZONY) -------------
// EN -> GPIO11 (GPIO22 NIE jest wyprowadzone na plytce). Firmware wystawia
// LOW przy starcie (grzalka ON) i mozna nia sterowac z www (zakladka
// Kalibracja). -1 = nieuzywany (wtedy EN musi byc fizycznie podpiety do GND).
#define PIN_MICS_EN          11

// --- Czujnik temperatury i wilgotnosci (DHT11, 1-Wire) -------------------------
// DHT11 USUNIETY - zamiast niego SHT31 (I2C #2). -1 = wylaczony.
#define PIN_DHT              -1

// --- Buzzer / przekaźnik (rezerwa) ------------------------------------------
#define PIN_BUZZER           -1

// --- Link do wezla C6 (programowy UART, etap 2) - piny ZAREZERWOWANE ----------
// Master nie ma wolnego sprzetowego UART-a (UART0=konsola, UART1=TVOC,
// UART2=PMS7003), wiec link bedzie EspSoftwareSerial (9600 b/s).
// Krzyzowanie: master RX(12) <- C6 TX(19), master TX(13) -> C6 RX(18).
#define PIN_NODE_RX          12
#define PIN_NODE_TX          13

// --- Przycisk BOOT ----------------------------------------------------------
#define PIN_BUTTON           0

// --- LCD lokalny: POMINIETY ---------------------------------------------------
// Decyzja: bez ekranu przy czujnikach (brak sensownej obudowy 3D na 7003+3,5").
// Obsluga i wyswietlanie ida przez panel Waveshare 7" (osobne urzadzenie),
// ktory w jednej z zakladek pokazuje strone www tego urzadzenia (Pulpit,
// Konfiguracja sieci, czujniki itp.). Piny ponizej zostaja wolne na rozbudowe.
#define PIN_LCD_CS           -1
#define PIN_LCD_DC           -1
#define PIN_LCD_RST          -1
#define PIN_LCD_BL           -1
#define PIN_TOUCH_CS         -1
#define PIN_TOUCH_IRQ        -1

// =============================================================================
//  UWAGA (N16R8):
//   - GPIO 26-37 zajete przez flash + PSRAM octal - NIE uzywac.
//   - GPIO 19/20 - natywny USB (nie uzywac jako GPIO zwykle).
//   - GPIO 43/44 - UART0 + mostek CP2102 (konsola).
//   - GPIO 45/46 - strapping / tylko wejscie.
//   - GPIO 3    - strapping JTAG.
//  Wolne zapasy: 1, 10, 41, 42.
//  (11 = MiCS EN, 40 = SCL2, 47 = SDA2, 48 = dioda RGB - nie uzywac.)
// =============================================================================
