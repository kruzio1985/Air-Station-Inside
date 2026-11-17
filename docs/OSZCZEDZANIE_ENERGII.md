# Oszczędzanie energii — analiza, wdrożenie i raport oszczędności

Dokument dotyczy stacji **192.168.1.141** (ESP32-S3 N16R8, firmware
`FilamentMonitor 1.0.0`). Punkt wyjścia: pomiar **13,2 W / 148 mA** na całej
grupie zasilaczy (230 V AC), z czego na samą stację przypada **~2,7 W średnio**
i **~4 W w szczycie**, a **~1,3–1,5 W** to grzałki MiCS5524 + MQ-7.

---

## 0. Bilans mocy (punkt odniesienia)

| Odbiornik | Szacowany prąd | Moc (5 V) | Uwaga |
|---|---|---|---|
| MQ-7 (grzałka 5 V, ciągle) | ~150 mA | ~0,75 W | brak pinu sterującego |
| MiCS5524 (grzałka + EN) | ~110–150 mA | ~0,55–0,75 W | EN = GPIO11 (aktywny niski) |
| Matryca LED 64 × WS2812B | 0–300 mA | 0–1,5 W | zależnie od trybu/jasności |
| PMS7003 (wentylator) | 30–50 mA | 0,15–0,25 W | tryb ciągły |
| TVOC 30712 | 20–40 mA | 0,10–0,20 W | strumień ramek 1 s |
| ESP32-S3 + WiFi (modem-sleep) | 40–80 mA @3,3 V | 0,13–0,26 W | zależy od ruchu w sieci |
| ADS1115, SHT31 ×2, SD, LED kontrolne | ~15 mA | ~0,07 W | patrz rozdz. 3 |
| **Razem (wyliczenie)** | | **~2,0–3,1 W** | zgodne z pomiarem 2,7 W |

Wniosek: budżet jest zdominowany przez **grzałki** (ok. 50–55 %) i **LED**
(0–35 %). Reszta (ESP32, PMS, TVOC) to kilkaset mW i nie da się jej istotnie
obniżyć bez pogorszenia funkcji pomiarowych.

Przeliczenie na stronę 230 V AC przy sprawności zasilaczy ~85 %:
`P_sieć ≈ P_5V / 0,85`.

---

## 1. LED (matryca 8×8 / pasek, dane = GPIO7) — WDROŻONE

### Co się zmieniło

1. **Dzień** — jasność ograniczona do konfigurowalnego procentu
   (`led_day_pct`, domyślnie **35 %**, dozwolone 1–40 %) **oraz** do limitu prądu
   (`led_max_ma`, domyślnie 1500 mA). Twardy limit elektryczny w kodzie: 40 %
   (`LED_BRIGHT_MAX_PCT`) — nawet wpisanie 200 w NVS nic nie da.
2. **Noc** — w oknie nocnym (`led_night_start` … `led_night_end`, domyślnie
   **22 → 6**, obsługa przejścia przez północ) matryca jest **całkowicie
   wyłączona**.
3. **Wyjątek nocny** — matryca włącza się tylko, gdy **jednocześnie**:
   - jakość powietrza przekracza próg: `IAQ ≥ led_alert_iaq` (100) **lub**
     `PM2.5 ≥ led_alert_pm25` (35 µg/m³) **lub** `CO₂ ≥ led_alert_co2` (1000 ppm),
   - **oraz** PIR (GPIO21) zgłasza ruch.

   Świeci przez `led_night_alert_s` sekund (domyślnie **60 s**) od ostatniego
   ruchu, potem gaśnie. Próg `0` wyłącza dany warunek.
4. **Optymalizacja transmisji** — `FastLED.show()` jest pomijany, gdy obraz się
   nie zmienił. Wcześniej tryb „powietrze" wysyłał 64 diody 50 × na sekundę
   (~96 ms/s zajętego CPU, przerwania wyłączone).
5. Wszystko jest w NVS i na stronie WWW (**Konfiguracja → „Oszczędzanie energii
   — LED (dzień / noc)"**), eksport/import konfiguracji też to obejmuje.

> Uwaga: nocne wyłączenie tłumi również **czerwony flash alarmu gazowego**
> (bez ruchu). To zachowanie zgodne z wymaganiem „noc — ring całkowicie OFF";
> alarmy pozostają widoczne w Home Assistant (MQTT). Aby alarm budził matrycę
> także bez ruchu, ustaw próg `led_alert_*` na poziom alarmowy i pozostaw
> `led_night_off = false`.

### Oszczędność

Model: WS2812B ≈ 20 mA na kanał przy 255. Tryb „buźka" zapala **42–46 z 64**
pikseli jednym kolorem.

| Stan | Prąd matrycy (5 V) | Moc |
|---|---|---|
| Dzień, 35 % (L≈89) | ~290–320 mA | ~1,45–1,60 W |
| Noc, 15 % (L≈38) — poprzednio | ~125–137 mA | ~0,63–0,69 W |
| Noc — **wyłączona** (teraz) | ~0 mA (tylko upływ ~4 mA) | **~0,02 W** |

- **Wyłączenie nocne: oszczędność ~0,6–0,7 W na wyjściu 5 V (~0,7–0,8 W na
  230 V AC)** w godzinach 22–6, czyli w praktyce ~1/3 doby → **≈0,25 W średnio**
  w skali doby.
- **Obniżenie jasności dziennej 40 % → 35 %**: ~40–45 mA → **~0,2–0,25 W** mniej
  w ciągu dnia (jeśli wcześniej efekt chodził na wyższej jasności).
- **Limit prądu 1500 mA** obcina jasne efekty (ikony, „ogień", pełna biel):
  maksymalnie ~1,5 A zamiast 3,8 A teoretycznych dla 64 diod.
- **Pomijanie niezmienionego `FastLED.show()`**: ~0,02–0,05 W oraz ~10 %
  czasu CPU w trybie statycznym (mniejsze opóźnienia MQTT/WWW).

⚠️ Pomiar kontrolny: w podsumowaniu całej stacji (2,7 W) udział LED nie może
przekraczać ~1 W. Jeśli po wyłączeniu matrycy (`POST /api/led
{"color":"000000","ms":600000}`) licznik nie pokaże wyraźnego spadku, matryca
w praktyce nie była zapalona na pełnej liczbie pikseli — model powyżej jest
górnym oszacowaniem.

---

## 2. Grzałki gazowe — analiza (NIE wdrożono cyklowania)

### MQ-7 (CO, A0 → ADS1115 AIN2)

- Nota katalogowa wymaga cyklu **60 s przy 5 V** + **90 s przy 1,4 V**; pomiar
  CO odczytuje się na końcu fazy 5 V.
- W tej instalacji **grzałka jest zasilona bezpośrednio z 5 V** (moduł 4-pin,
  brak wyprowadzonego sterowania grzałką, `D0` nieużywane). **Nie ma pinu,
  którym firmware mógłby przełączać grzałkę** — duty cycle wymagałby
  MOSFET-a/przekaźnika i dzielnika napięcia, czyli zmian sprzętowych i pinów
  (poza zakresem tego zadania).
- Zysk energetyczny, gdyby sprzęt powstał: średnie napięcie
  `(60·5 + 90·1,4)/150 = 2,84 V` → moc ~0,24 W zamiast 0,75 W, czyli
  **~0,5 W**. To największa pojedyncza oszczędność w całej stacji — ale
  wyłącznie sprzętowa.

### MiCS5524 (EN = GPIO11, aktywny niski — sprawdzone w kodzie)

- `ads_begin()` ustawia **EN = LOW** (grzałka ON) i tak zostaje; `ads_set_heater()`
  pozwala ją wyłączyć z WWW (zakładka Kalibracja). Polaryzacja jest poprawna.
- Cyklowanie EN jest **technicznie możliwe**, ale:
  - wymagane wygrzewanie to **~15 min** (`MICS_WARMUP_MS`), a po odcięciu
    zasilania czujnik wraca do stabilności **10–15 min**,
  - R0 (kalibracja) **dryfuje** po każdym zimnym starcie; przy cyklu dobowym
    trzeba by kalibrować ponownie,
  - Rs/R0 i wyliczane CO/VOC są wtedy niewiarygodne przez większość czasu,
  - żywotność grzałki spada wraz z liczbą cykli termicznych.

### Kompromisy (podsumowanie)

| Podejście | Oszczędność | Baseline / R0 | Dokładność CO/VOC | Żywotność | Ocena |
|---|---|---|---|---|---|
| Ciągłe 5 V (stan obecny) | 0 W | stabilny R0 | pełna | najlepsza | ✅ zostaje |
| MiCS EN 10 min ON / 30 s OFF | ~0,03 W | lekki dryf | ~10 % czasu bez pomiaru | gorsza | ❌ nie warto |
| MiCS EN 15 min ON / 5 min OFF | ~0,15 W | wyraźny dryf | wymaga re-warmupu 10–15 min → pomiar praktycznie ciągle „warmup" | zła | ❌ psuje pomiar |
| MQ-7 60 s/90 s (5 V / 1,4 V) | ~0,5 W | stabilny | dobra (zgodna z notą) | dobra | ⚠️ tylko po dodaniu sterowania grzałką |

**Decyzja:** grzałki pozostają na stałym zasilaniu. CO/VOC (i wynikający z nich
alarm gazowy MiCS) nie są pogarszane. Ewentualny zysk ~0,65 W wymaga
przebudowy sprzętowej (osobny tor zasilania grzałki MQ-7 sterowany z GPIO).

---

## 3. Czerwone diody LED na płytkach — co da się zgasić z firmware

| Element | Dioda | Sterowalna z firmware? | Prąd |
|---|---|---|---|
| ESP32-S3-DevKitC-1 | czerwona **PWR** (3,3 V) | **NIE** — na stałe do szyny 3,3 V | ~1–2 mA / 0,005 W |
| ESP32-S3-DevKitC-1 | adresowalna RGB (GPIO48) | teoretycznie TAK, ale firmware jej nie inicjuje (jest zgaszona) | 0 mA |
| Moduł ADS1115 | czerwona PWR | **NIE** (na stałe, rezystor do VCC) | ~1–2 mA |
| Moduł MQ-7 | czerwona PWR (+ D0) | **NIE** | ~2–5 mA |
| Moduł MiCS5524 | zwykle brak lub PWR | **NIE** | ~1–2 mA |
| Waveshare TVOC 30712 | PWR (jeśli zamontowana) | **NIE** | ~1–3 mA |
| Moduł microSD (SPI) | PWR za LDO 3,3 V | **NIE** | ~1–3 mA |
| PMS7003 | brak diody sygnalizacyjnej | — | — |

**Razem ~6–17 mA @5 V ≈ 0,03–0,08 W** — nie do wyłączenia z firmware.
Jedyna droga to fizyczne usunięcie diody/rezystora na płytce modułu
(nie robimy tego w tym zadaniu; przy 0,03–0,08 W zysk jest marginalny).

Sprawdzenie pinu EN MiCS5524: `pins.h` → `PIN_MICS_EN 11`, aktywny **niski**;
`ads_begin()` wymusza `LOW`, `ads_set_heater(false)` daje `HIGH`. Stan jest
raportowany w `/api/status` (`mics_heater`) i widoczny na stronie w zakładce
Kalibracja — brak błędów polaryzacji.

---

## 4. PMS7003, TVOC, SD, interwały

### PMS7003 (UART2, 9600; SET podpięty do 3V3 = tryb ciągły)

- **Korekta nazewnictwa:** w rodzinie Plantower **`0xE4` to WYBUDZENIE**,
  a **`0xE3` to SEN** (`0xE1` — tryb pasywny, `0xE2` — żądanie odczytu).
  Samo wysłanie `0xE4` nic nie uśpi.
- Linia RXD czujnika jest podłączona (GPIO17), więc komendy da się wysłać, ale
  przy **SET = 3V3** moduł pracuje w trybie ciągłym i nie ma gwarancji, że
  przyjmie przejście w sen — bez czujnika na biurku nie da się tego
  zweryfikować, a błędne przejście = **utrata danych PM**.
- Zysk: ~0,10–0,15 W przy 50 % duty, ale:
  - `agg.cpp` po **15 s** bez ramki uznaje PM za nieaktualny → IAQ, LED i JSON
    dla stacji pogodowej tracą PM (fioletowa matryca, brak pól w JSON),
  - średnie 1 min / 5 min / 24 h miałyby dziury.
- **Decyzja: nie wdrażamy.** Stosunek zysku (0,1 W) do ryzyka (utrata pomiaru
  PM, który jest podstawą IAQ) jest zły.

### Waveshare TVOC 30712 (UART1, 115200)

- Moduł sam nadaje ramkę co 1 s; **nie ma udokumentowanej komendy snu**
  (tylko tryb Q&A, który nie zmniejsza poboru — własny MCU i grzałka pracują).
- Pobór ~0,1–0,2 W — **nie da się obniżyć z firmware** bez odcinania zasilania
  (brak pinu i brak MOSFET-a).

### Karta SD

- Zapis paczki rekordów co **5 min** (`SD_LOG_INTERVAL_MS`). Karta w spoczynku
  pobiera ~1–2 mA (~0,01 W) — wydłużanie interwału lub `SD.end()` między
  zapisami nie daje mierzalnej oszczędności, a zwiększa ryzyko utraty danych.
  **Bez zmian.**

### Interwały odczytu i publikacji

| Co | Wartość | Zmiana |
|---|---|---|
| PMS7003 | ramka co ~1 s (sprzętowo) | bez zmian |
| TVOC 30712 | ramka co ~1 s (sprzętowo) | bez zmian |
| ADS1115 (MiCS/MQ-7) | próbka co 0,5 s, mediana 10 próbek | bez zmian |
| SHT31 / DHT | cykl własny sterownika | bez zmian |
| HA (temp/wilg. zewn.) | 60 s | bez zmian |
| HA (obecność) | 10 s — **tylko gdy `presence_led` jest włączone** | bez zmian |
| **MQTT — stan kanałów** | **10 s (było 5 s), konfigurowalne 5–300 s** | **wdrożone** |
| MQTT — JSON powietrza (retained) | 30 s | bez zmian |
| Zapis SD | 5 min | bez zmian |

Skrócenie publikacji MQTT nie daje zauważalnej oszczędności prądu (radio i tak
pracuje w modem-sleep; transmisja to kilka ms na cykl), ale **zmniejsza ruch
w sieci i obciążenie brokera o połowę**. Jeśli wolisz szybszą reakcję HA —
ustaw interwał z powrotem na 5 s.

---

## 5. Podsumowanie oszczędności

| # | Zmiana | Status | Oszczędność (5 V) | Oszczędność (230 V) | Uwagi |
|---|---|---|---|---|---|
| 1 | Nocne **wyłączenie** matrycy (22–6) | ✅ wdrożone | 0,63–0,69 W przez ~8 h | **~0,7–0,8 W** w oknie, **~0,25 W średnio** | wyjątek: zła jakość + ruch |
| 2 | Limit jasności dziennej 40 % → 35 % | ✅ wdrożone | ~40–45 mA (~0,2 W) | ~0,25 W | tylko gdy efekt jaśniejszy niż limit |
| 3 | Limit prądu 1500 mA + twardy sufit 40 % | ✅ wdrożone | obcina szczyty do ~1,5 A | j.w. | ochrona zasilacza LED |
| 4 | Pomijanie niezmienionego `FastLED.show()` | ✅ wdrożone | 0,02–0,05 W | ~0,03 W | + ~10 % CPU w trybie statycznym |
| 5 | Interwał MQTT 5 s → 10 s | ✅ wdrożone | ~0,01–0,02 W | ~0,02 W | głównie mniejsze obciążenie brokera |
| 6 | Cykl grzałki MQ-7 (nota 60/90 s) | ⛔ wymaga sprzętu | ~0,5 W | ~0,6 W | brak pinu sterującego grzałką |
| 7 | Cykl grzałki MiCS5524 (EN) | ⛔ odrzucone | 0,03–0,15 W | — | psuje R0 i pomiar CO/VOC |
| 8 | Sen PMS7003 (0xE3) | ⛔ odrzucone | ~0,1–0,15 W | ~0,15 W | utrata PM/IAQ w cyklu |
| 9 | Wyłączenie czerwonych LED na modułach | ⛔ niemożliwe | 0,03–0,08 W | — | tylko fizyczna ingerencja |

**Realny efekt wdrożenia:** ~**0,3–0,6 W średnio** (≈ **0,35–0,7 W na 230 V**),
czyli **13–26 %** zużycia stacji przy założeniu 2,7 W średnio — w praktyce
prawie cały zysk pochodzi z punktu 1 (noc) i 2 (dzień). Wartości trzeba
potwierdzić licznikiem (rozdz. 6), bo model LED zależy od tego, ile pikseli
faktycznie świeci.

---

## 6. Jak zweryfikować pomiar

1. `GET /api/status` → nowe pola:
   - `led_night` (1 = okno nocne), `led_night_off`, `led_spared`
     (1 = matryca wyłączona oszczędnościowo), `led_day_pct`, `led_night_pct`,
     `led_eff_level` (efektywna jasność 0–255).
2. Wymuszone wyłączenie matrycy na 10 min (do porównania z licznikiem):
   ```
   curl -X POST http://192.168.1.141/api/led -d '{"color":"000000","ms":600000}'
   ```
3. Log w konsoli (115200): `LED dbg: br=<poziom> max=<max piksel> (n=64)` —
   widać, czy matryca realnie świeci i na jakim poziomie.
4. Pomiar mocy: licznik gniazdkowy, 1 h w dzień i 1 h w nocy, z włączonym
   i wyłączonym nocnym oszczędzaniem (`led_night_off`).

---

## 7. Nowe klucze NVS (bez zmiany istniejących)

Zapis w namespace `filament` (jak dotychczas). **Żadne istniejące klucze nie
zostały zmienione ani usunięte** — nowe mają domyślne wartości, więc urządzenie
po aktualizacji zachowuje całą konfigurację.

| Klucz | Typ | Domyślnie | Znaczenie |
|---|---|---|---|
| `led_day` | int | 35 | limit jasności dzień [%] (1–40) |
| `led_night` | int | 15 | limit jasności noc [%] (1–40) |
| `led_night_off` | bool | true | całkowite wyłączenie w oknie nocnym |
| `led_n_start` | int | 22 | początek okna nocnego (godzina) |
| `led_n_end` | int | 6 | koniec okna nocnego (godzina) |
| `led_n_alert` | int | 60 | czas podświetlenia po ruchu [s] |
| `led_a_iaq` | int | 100 | próg IAQ (0 = ignoruj) |
| `led_a_pm` | float | 35.0 | próg PM2.5 [µg/m³] |
| `led_a_co2` | int | 1000 | próg CO₂ [ppm] |
| `m_ivl` | int | 10000 | interwał publikacji MQTT [ms] (5000–300000) |

Eksport/import (`/api/backup`, `/api/config`) zawiera te pola jako
`led_day_pct`, `led_night_pct`, `led_night_off`, `led_night_start`,
`led_night_end`, `led_night_alert_s`, `led_alert_iaq`, `led_alert_pm25`,
`led_alert_co2`, `mqtt_interval_ms`.

---

## 8. Czego nie ruszaliśmy

- **Piny** — bez zmian ([include/pins.h](../include/pins.h)).
- **NVS** — żadnego resetu, czyszczenia ani zmiany istniejących kluczy;
  dodane tylko nowe (z domyślnymi wartościami).
- **Pomiary** — PM, IAQ, korekcja RH, kalibracje R0, progi alertów, alarm
  gazowy: bez zmian. Jedyny wpływ na LED to jasność i nocne wyłączenie.
- **OTA** — nie wykonano żadnego wgrania na urządzenie.
