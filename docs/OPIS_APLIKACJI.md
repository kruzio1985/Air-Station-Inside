# Stacja Powietrza — kompletny opis aplikacji

Stacja master jakości powietrza na **ESP32-S3 WROOM-1 N16R8** (PlatformIO,
Arduino). Zbiera dane z czujników, filtruje je, agreguje do rekordów minutowych,
wysyła do MQTT/Home Assistant, loguje na SD i wyświetla na stronie WWW.

## 1. Sprzęt i piny

Pełna mapa pinów: [include/pins.h](../include/pins.h).

| Czujnik | Interfejs | Piny |
|---|---|---|
| PMS7003 (PM1/2.5/10) | UART2, 9600 | RX=18, TX=17 |
| Waveshare TVOC 30712 (CO₂/TVOC/CH₂O) | UART1, 115200 | RX=14, TX=15 |
| MiCS5524 (gaz, przez ADS1115) | I2C, 0x48 | SDA=8, SCL=9, AIN0; EN=GPIO11 |
| MQ-7 (CO, przez ADS1115) | analog | AIN2 (A0→AIN2), 5 V |
| SHT31 #1 (temp/wilgotność wewn.) | I2C#2, 0x44 | SDA=47, SCL=40 |
| SHT31 #2 (temp/wilgotność zewn.) | I2C, 0x44 | SDA=8, SCL=9 (obok ADS1115) |
| DHT11 (temp/wilgotność — USUNIĘTY) | 1-Wire | — (wyłączony) |
| Karta microSD | SPI | SCK=4, MOSI=5, MISO=6, CS=2 |
| Pasek LED WS2812B | 1-wire | GPIO7 |
| PIR (ruch) | GPIO | 21 |

## 2. Architektura danych

```
CZUJNIKI (PMS ~1 s, TVOC ~1 s, DHT11 ~2 s, MiCS, HA ext ~60 s)
   │
   ▼  sensors.cpp
- mediana PM (okno 30 s)                 → wartość na ekran
- korekcja wilgotnościowa PM (RH>próg)  → PM_corr = PM / (1+k·(RH-próg)/(100-próg))
   │  (RH z zewnętrznego czujnika HA; fallback = DHT11)
   ▼  agg.cpp
- akumulacja 1 min → rekord minutowy (średnie)
- ring buffer 1440 rekordów = 24 h
- średnie 1 min / 5 min / 24 h
   │
   ├──► SD (co 5 min, paczkami) → /logs/stacja_powietrza-YYYY-MM.csv
   ├──► MQTT (co 5 s kanały + co 30 s JSON powietrza)
   ├──► WWW (pulpit, wykresy, API)
   └──► Home Assistant (MQTT discovery, 1 urządzenie „Stacja Powietrza")
```

Po restarcie bufor 24 h jest **odtwarzany z dzisiejszego CSV** na SD.

## 3. Kanały (sensory)

`pm1, pm25, pm10, aqi, co2, ch2o, tvoc, tvoclvl, mics_v, mics_rs, mics_co,
mics_wu, motion, temp, hum, pm25_1m, pm25_5m, pm25_24h, temp_24h, hum_24h,
tempext, humext, rhcorr`.

## 4. Korekcja wilgotnościowa PM

Optyczny PMS7003 zawyża PM przy wysokiej wilgotności. Stacja koryguje:

```
PM_corr = PM_raw / (1 + k · (RH − próg) / (100 − próg))    dla RH > próg
```

- próg domyślny: **60 %**, współczynnik **k = 0.5** (konfigurowalne),
- RH do korekcji: **zewnętrzny czujnik HA** (dokładny); fallback — wewnętrzny DHT11,
- status korekcji: kanał `rhcorr` + napis na pulpicie.

## 5. Zewnętrzny czujnik HA

Stacja co 60 s odczytuje temp/wilgotność z czujnika Tuya przez REST API HA
(`GET /api/states/<entity>`, token Bearer). Konfiguracja: URL, token, entity.

## 6. MQTT / Home Assistant

- Discovery: `homeassistant/sensor/stacja_powietrza_<id>/config`
- Stan kanałów: `stacja_powietrza/sensor/<id>/state`
- **JSON powietrza dla Stacji Pogodowej**: `stacja_powietrza/air` (patrz
  [INTERFEJS_STACJA_POGODOWA.md](INTERFEJS_STACJA_POGODOWA.md))
- W HA: **jedno urządzenie „Stacja Powietrza"** z wieloma czujnikami.

## 7. Pasek LED

Wyłączony bez ruchu; zapala się po wykryciu ruchu (PIR/Tuya) na czas podtrzymania.
Kolor to **trzypoziomowa ogólna ocena powietrza** (nie najgorszy pojedynczy
parametr): 🟢 zielony = dobre, 🟡 żółty = podwyższone utrzymujące się >5 min,
🔴 czerwony = poważnie podwyższone. Zmiana poziomu wymaga utrzymania warunku
przez 5 minut (z histerezą), więc pojedynczy lekko podwyższony parametr nie
powoduje migania. Czerwony FLASH = alarm krytyczny (wykryty gaz MiCS po
wygrzaniu / awaria czujników) — widoczny zawsze, niezależnie od ruchu.
W nocy (22–6) przyciemniony.

## 8. Strona WWW

Zakładki: **Pulpit** (bieżące odczyty + IAQ + temp/wilgotność wewn./zewn.),
**Wykresy** (PM2.5/PM10, temp/wilgotność — 12 h), Filament, LED, Kalibracja,
Konfiguracja, Logi, SD, Diagnostyka, OTA.

## 9. REST API

`/api/status`, `/api/config` (GET/POST), `/api/history?hours=N`, `/api/diag`,
`/api/syslog`, `/api/sd/list|file|delete`, `/api/calibrate`, `/api/led`,
`/api/ota`, `/api/file` (upload pliku strony bez USB), `/api/reboot`,
`/api/factory`, `/api/backup`.

## 10. Budowanie i wgrywanie

```
pio run -e esp32s3 -t upload            # firmware (USB)
pio run -e esp32s3 -t uploadfs          # strona (USB) — lub /api/file przez HTTP
pio run -e esp32s3-ota -t upload --upload-port 192.168.1.141   # firmware OTA
```

## 11. Konfiguracja (NVS, edytowalna na stronie)

Wi-Fi, MQTT, HA (URL/token/entity), korekcja PM-RH, LED, progi, PIR, pojemniki
filamentu, kalibracja MiCS. Wartości domyślne: `src/nvs_store.cpp`.
