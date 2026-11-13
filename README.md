# StacjaPowietrza — stacja master jakości powietrza

*(dawniej „FilamentMonitor" — monitor powietrza i wilgotności filamentu przy drukarce 3D)*

> **Kompletny opis aplikacji:** [docs/OPIS_APLIKACJI.md](docs/OPIS_APLIKACJI.md) ·
> **Interfejs MQTT dla Stacji Pogodowej:** [docs/INTERFEJS_STACJA_POGODOWA.md](docs/INTERFEJS_STACJA_POGODOWA.md)

Monitor przy drukarce 3D na **ESP32-S3 WROOM-1 N16R8** (osobna płytka, niezależna
od stacji pogody). Etap 1: firmware mastera ze sterownikami czujników powietrza,
stroną WWW, MQTT/Home Assistant, kartą SD i paskiem LED RGBIC.

## Sprzęt mastera (etap 1)

| Czujnik | Interfejs | Piny |
|---|---|---|
| PMS7003 | UART2, 9600 | RX=18, TX=17 |
| Waveshare TVOC (SKU 30712) | UART1, 115200 | RX=14, TX=15 |
| ADS1115 (0x48) + MiCS5524 | I2C | SDA=8, SCL=9 |
| Karta microSD | SPI | SCK=4, MOSI=5, MISO=6, CS=2 |
| Pasek LED RGBIC (WS2812B) | 1-wire | dane=7 |
| DHT11 (temp./wilgotność) | 1-Wire | DATA=10 |
| Ruch lokalny (PIR/LD2410) | GPIO | 21 |

Pełna mapa w [include/pins.h](include/pins.h). Uwagi N16R8: GPIO 26–37 zajęte
(flash + PSRAM octal), 19/20 = natywny USB, 43/44 = UART0 + mostek, 45/46 = strapping.

## MiCS5524 → ADS1115 (ważne)

ADS1115 zasilany z **5 V (wymagane)** → wejście analogowe 0–5 V, więc wyjście
MiCS5524 (0–5 V) idzie **wprost do AIN0, bez dzielnika**. Gain ±6,144 V.
Kalibracja R0 (czyste powietrze) w zakładce **Kalibracja**.
Moduł 4-pin: VCC(5 V), GND, A0 (analogowe), **EN** (włączanie czujnika,
**aktywny niski**: EN→GND = grzałka ON, EN→5V = grzałka OFF).

> ⚠️ Uwaga na I2C: ADS1115 na 5 V ma SDA/SCL podciągnięte do 5 V, a ESP32-S3 to
> 3,3 V. Zwykle działa wprost; przy problemach (`brak odpowiedzi na 0x48`)
> dołóż 2-kanałowy konwerter poziomów I2C (BSS138).

Pełna instrukcja lutowania i połączeń: [docs/LUTOWANIE.md](docs/LUTOWANIE.md).

## Budowanie i wgrywanie

```
pio run -e esp32s3 -t upload     # firmware
pio run -e esp32s3 -t uploadfs   # strona www (LittleFS)
pio device monitor               # konsola (USB CDC)
```

OTA: `pio run -e esp32s3-ota -t upload --upload-port <IP>` (hasło: ustaw w config.h).

Pierwsze uruchomienie bez zapisanego Wi-Fi: stacja startuje jako punkt dostępowy
**FilamentMonitor** / (hasło AP) → http://192.168.4.1 → zakładka **Konfiguracja**.

## Home Assistant / MQTT

Discovery: `homeassistant/sensor/<prefix>_<id>/config`, stan `<prefix>/sensor/<id>/state`.
Ustawienia brokera na stronie (Konfiguracja). Pojemniki na filament (etap 2) przyjdą
przez subskrypcję tematu `zigbee2mqtt/FilamentBox` (klucze `boxN_temperature`,
`boxN_humidity`). Sterowanie paskiem z HA: publikacja na `<prefix>/led/mode` (`motion`).

## REST API

| Metoda | Ścieżka | Opis |
|---|---|---|
| GET | `/api/status` | kanały, pojemniki, system |
| GET/POST | `/api/config` | ustawienia (JSON) |
| GET | `/api/backup` | kopia ustawień |
| POST | `/api/backup/restore` | przywrócenie ustawień |
| POST | `/api/calibrate` | zapisz R0 MiCS5524 |
| POST | `/api/led` | test koloru paska |
| GET/DELETE | `/api/syslog` | logi |
| POST | `/api/reboot` / `/api/factory` | restart / reset fabryczny |

## Karta SD

`/logs/filament-YYYY-MM.csv` — pomiary co 60 s (kanały + pojemniki).

## Etapy

- [x] **1** — master: PMS7003, TVOC, ADS1115+MiCS5524, SD, LED, www, MQTT/HA
- [ ] **2** — węzeł ESP32-C6: TCA9548A + 8× AHT20, Zigbee (EP1–EP8) → Zigbee2MQTT
- [ ] **3** — integracja: pojemniki na pulpicie, segmenty LED, ruch PIR/Tuya
- [ ] **4** — panel Waveshare 7" jako HMI (kiosk) — patrz niżej

## Obsługa i wyświetlanie (panel Waveshare 7")

Bez lokalnego ekranu przy czujnikach — obsługa idzie przez panel **Waveshare 7"**.
Strona www tego urządzenia ma być **jedną z zakładek** panelu i zawierać:
pulpit czujników, konfigurację sieci/MQTT, kalibrację i logi.

- Zwykły adres: `http://filament-monitor.local/` (lub IP stacji)
- Tryb kiosk (sam pulpit, bez menu): `http://<IP>/?kiosk=1`
- Interfejs odświeża się sam co 3 s (fetch `/api/status`), więc panel pokazuje
  dane na żywo bez przeglądarki — wystarczy zwykły widok WWW/iframe na panelu.
- Dla paneli bez przeglądarki (LVGL na ESP32) dane są dostępne jako JSON:
  `GET /api/status` (kanały + pojemniki + system).
