# Interfejs MQTT — dane jakości powietrza dla Stacji Pogodowej

Stacja Powietrza publikuje **zbiorczy JSON jakości powietrza** na MQTT, aby
Stacja Pogodowa (lub dowolny inny odbiorca) mogła odczytywać powietrze w domu.
**Ten dokument to kompletny opis interfejsu — wystarczy do napisania kodu
odbiorcy w osobnym czacie.**

## Broker

| Parametr | Wartość |
|---|---|
| Host | `192.168.1.14` |
| Port | `1883` |
| Użytkownik | `CHANGE_ME` |
| Hasło | w SEKRETY: `HISTORIA\SEKRETY\stacja_powietrza_mqtt.txt` |

## Temat

```
stacja_powietrza/air
```

- publikowany **co 30 s**,
- flaga **retained** (nowy subskrybent od razu dostaje ostatnią wartość),
- temat można zmienić w Konfiguracji stacji („Temat JSON powietrza").

## Format (JSON)

Wszystkie pola poza `ts` są **opcjonalne** — pojawiają się tylko wtedy, gdy
dany czujnik ma poprawny odczyt (`valid`).

| Pole | Jednostka | Opis |
|---|---|---|
| `ts` | unix s | czas publikacji (0 = brak synchronizacji NTP) |
| `pm1` | µg/m³ | PM1.0 (PMS7003, atmospheric environment, po medianie i korekcji RH) |
| `pm25` | µg/m³ | PM2.5 (jak wyżej) |
| `pm10` | µg/m³ | PM10 (jak wyżej) |
| `aqi` | 0–500 | IAQ — najgorszy z PM2.5/PM10/CO₂/TVOC/CH₂O (orientacyjny) |
| `co2` | ppm | CO₂ (Waveshare TVOC 30712) |
| `tvoc` | ppm | TVOC (Waveshare 30712) |
| `ch2o` | ppb | formaldehyd (Waveshare 30712) |
| `mics_co` | ppm | ekwiwalent CO (MiCS5524, NIESELEKTYWNY, orientacyjny) |
| `mq7_co` | ppm | CO z MQ-7 (orientacyjny, krzywa przybliżona, wymaga kalibracji R0) |
| `tvoc_level` | 1–4 | poziom jakości powietrza z modułu 30712 (1 = najlepszy) |

> **Świadomie NIE ma tu temperatury i wilgotności** — Stacja Pogodowa ma własne
> czujniki temp/RH. Temp/wilgotność stacji są dostępne osobno (patrz niżej).

### Przykład

```json
{
  "ts": 1767823200,
  "pm1": 12.0,
  "pm25": 21.0,
  "pm10": 23.0,
  "aqi": 70,
  "co2": 441.0,
  "tvoc": 0.104,
  "ch2o": 0.0,
  "mics_co": 1.2,
  "tvoc_level": 1
}
```

## Uwagi dla odbiorcy (Stacja Pogodowa)

1. Subskrybuj temat `stacja_powietrza/air`; dzięki `retained` dostaniesz
   ostatni stan natychmiast po połączeniu (nie czekaj na kolejne 30 s).
2. Jeżeli pola `pm1/pm25/pm10/co2/...` **nie ma** w JSON — czujnik nie ma
   jeszcze poprawnego odczytu (np. rozgrzewanie MiCS → `mics_co` zniknie).
3. `mics_co` to wartość **orientacyjna** (czujnik nieselektywny) — nie używaj
   jako alarmu CO.
4. `aqi` to **IAQ** (indeks jakości powietrza wewnętrznego): najgorszy z
   parametrów PM2.5 / PM10 / CO₂ / TVOC / CH₂O, 0–500. To klasyfikacja
   orientacyjna, nie oficjalny indeks US EPA.

## Pozostałe tematy MQTT stacji (opcjonalnie)

- Stan pojedynczych kanałów (w tym **temp/hum**):
  `stacja_powietrza/sensor/<id>/state`
  (id m.in.: `temp`, `hum`, `tempext`, `humext`, `pm25_1m`, `pm25_5m`, `pm25_24h`)
- Discovery Home Assistant: `homeassistant/sensor/stacja_powietrza_<id>/config`
