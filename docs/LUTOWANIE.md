# FilamentMonitor — instrukcja lutowania i połączeń (master ESP32-S3)

Wszystkie piny w [include/pins.h](../include/pins.h). Połączenia „po imieniu"
są ważniejsze niż kolory — trzymaj się tabel poniżej.

## Narzędzia

- lutownica (ok. 320–350 °C), cyna 0,6–0,8 mm z topnikiem, topnik/ kalafonia,
- cienkie przewody (AWG24–28), skrętka (CAT5) na I2C/UART,
- koszulki termokurczliwe, miernik (ciągłość/omomierz),
- opcjonalnie: listwy goldpin 2,54 mm, płytka uniwersalna.

---

## 1. PMS7003 — złącze mikro 2×5 (10 pinów, raster 1,27 mm)

To jest **jedyny** element z małym złączem. Pinout wg datasheet v2.5.

**Fizycznie** (patrząc na otwory złącza, pin 1 w rogu z kluczem/kropką; numeracja
kolumnami — lewa kolumna 1,3,5,7,9, prawa 2,4,6,8,10):

```
  koniec zasilania                    koniec sygnałów
  ┌──────────────┬──────────────┐
  │ 1  VCC (5V)  │ 2  VCC (5V)  │   ← oba do 5 V
  │ 3  GND       │ 4  GND       │   ← oba do GND
  │ 5  RESET     │ 6  NC        │
  │ 7  RXD       │ 8  NC        │
  │ 9  TXD       │ 10 SET       │   ← 9 = dane, 10 = tryb
  └──────────────┴──────────────┘
```

Jak rozpoznać bez opisów: **4 piny przy jednym końcu to zasilanie** (2×VCC + 2×GND),
**6 pinów przy drugim końcu to sygnały** (RESET, RXD, TXD, SET + 2×NC). Pin 1 jest
przy rogu z kluczem/wybrzuszeniem na obudowie złącza (sprawdź też ciągłość miernikiem:
oba VCC są ze sobą zwarte, oba GND też).

| Pin | Sygnał | Podłącz do | Uwaga |
|---|---|---|---|
| 1 | **VCC** | 5 V | zasilanie wentylatora (musi być 5 V!) |
| 2 | **VCC** | 5 V | to samo (można zewrzeć z pin 1) |
| 3 | **GND** | GND | |
| 4 | **GND** | GND | |
| 5 | **RESET** | 3,3 V (lub 10 k do 3V3) | aktywny niski; wewnętrznie podciągnięty |
| 6 | NC | — | zostaw niepodłączony |
| 7 | **RXD** | GPIO 17 (opcjonalnie) | wejście czujnika; tylko tryb pasywny/komendy |
| 8 | NC | — | zostaw niepodłączony |
| 9 | **TXD** | **GPIO 18** (RX ESP32) | ⭐ **to jest cyfrowe wyjście danych** (PM1/2.5/10) |
| 10 | **SET** | 3,3 V | tryb ciągły (pomiar non-stop) |

**Które wyjście cyfrowe się wykorzystuje:** **TXD (pin 9)** — po UART (9600 Bd,
logika 3,3 V) czujnik wysyła ramki 32 B z PM1.0/PM2.5/PM10. Idzie ono na
**RX ESP32 = GPIO 18**. RXD (pin 7) to wejście czujnika — potrzebne tylko
do trybu pasywnego i komend (u nas opcjonalne).

**Jak lutować:**
1. **Najlepiej:** kup/zlutuj adapter PMS7003 (płytka z gniazdem 1,27 mm 2×5
   i wyprowadzeniami goldpin 2,54 mm z opisem `VCC GND RXD TXD SET RESET`).
   Wtedy lutujesz tylko do dużych pinów adaptera, a czujnik wpinasz do gniazda.
2. **Bezpośrednio (bez adaptera i kabelka):** patrz niżej.

### Lutowanie bezpośrednie do mikro-pinów (gdy nie masz kabelka ani adaptera)

**Minimum 4 przewody** — tyle wystarczy, żeby czujnik działał:

| Sygnał | Pin | Do ESP32 |
|---|---|---|
| VCC | 1 | 5 V |
| GND | 3 | GND |
| TXD | 9 | GPIO 18 |
| SET | 10 | 3,3 V |

RESET (5) możesz zostawić niepodłączony (wewnętrzny pull-up), RXD (7) tylko do
trybu pasywnego, NC (6,8) pomijasz.

**Jak rozpoznać piny bez opisów (miernik, tryb „buzzer/ciągłość"):**

1. **Koniec zasilania:** znajdź **dwie pary zwartych pinów** (miernik piszczy).
   Są tylko dwie takie pary i leżą przy jednym końcu złącza:
   - para 1–2 = VCC,
   - para 3–4 = GND.
2. **Odróżnij VCC od GND:** przyłóż sondę do **metalowej obudowy** (srebrny kubek
   czujnika). Para, która „piszczy" z obudową = **GND**. Druga = **VCC**.
   Jeśli obudowa nie piszczy z żadną: popatrz na spód PCB — GND ma najszerszą
   ścieżkę / dużą powierzchnię miedzi, VCC biegnie do wentylatorka.
3. **Znajdź TXD (pin 9):** po podłączeniu zasilania czujnik w trybie aktywnym
   **ciągle nadaje** po UART. Miernik w trybie DC pokaże na TXD **~1,5–1,7 V**
   (średnia z szybkiego strumienia bitów), a pozostałe piny sygnałowe 0 lub 3,3 V
   stałe. Najpewniejsze: podpinaj po kolei podejrzane piny przez rezystor 1 kΩ do
   GPIO 18 i patrz w log na COM13 — jak zobaczysz `PMS: PM1.0=...`, to jest TXD.

**Technika lutowania (raster 1,27 mm):**

- przewód cienki **AWG30 (wire-wrap)** albo pojedyncza żyła z taśmy, koniec
  ocynowany na 2–3 mm,
- grot cienki, **320–330 °C**, **dużo topnika** (kalafonia/żel), czas styku
  **max 2–3 s** na pin,
- lutuj **po jednym pinie**, od końca zasilania (większy odstęp), potem sygnały,
- po każdym pinie sprawdź **lupą**, że nie ma mostka cyny między sąsiednimi
  pinami (zwarcie VCC–GND spali czujnik),
- piny są delikatne — nie ruszaj nimi, dopóki cyna nie zastygnie,
- na końcu miernikiem sprawdź brak zwarcia 5 V ↔ GND przed włączeniem.

> ⚠️ NIE zasilaj PMS7003 z 3,3 V — wentylator potrzebuje 5 V. UART to 3,3 V TTL,
> więc do ESP32 idzie wprost, bez konwertera.

---

## 2. MiCS5524 — moduł 4-pin (VCC, GND, A0, EN)

| Pin modułu | Podłącz do | Uwaga |
|---|---|---|
| **VCC** | 5 V | grzałka czujnika |
| **GND** | GND | |
| **A0** (wyjście analogowe) | **wprost → AIN0** ADS1115 | 0–5 V, bez dzielnika |
| **EN** | **GND** (zawsze włączony) **lub** GPIO 11 | EN = włączanie czujnika (**aktywny niski**: LOW/GND = grzałka ON, HIGH = OFF) |

- **EN** to pin włączający czujnik, **nie** wyjście cyfrowe. Jest **aktywny
  niski**: podłącz do **GND**, żeby grzałka grzała cały czas (do monitoringu
  ciągłego), albo do GPIO 11 (`PIN_MICS_EN`) — firmware wystawi tam **LOW** przy
  starcie (grzałka ON), a z www (zakładka Kalibracja) możesz ją włączać/wyłączać.
- Zakresy: CO 1–1000 ppm, NH₃ 1–500 ppm, etanol 10–500 ppm, H₂ 1–1000 ppm,
  węglowodory ~1000 ppm. Czujnik jest **nieselektywny** (nie odróżnia gazów)
  i wymaga ~15 min wygrzewania.

### Podłączenie (ADS1115 na 5 V — bez dzielnika)

ADS1115 zasilany z **5 V** ma wejście analogowe **0–5 V**, więc wyjście MiCS
(0–5 V) idzie **wprost**:

```
MiCS A0 ────────────── AIN0 (ADS1115)     kondensator 100 nF z AIN0 do GND
MiCS VCC ── 5 V                           (opcjonalnie, wygładza sygnał)
MiCS GND ── GND
```

> ⚠️ **Jedna rzecz do pilnowania — I2C.** ADS1115 na 5 V ma linie SDA/SCL
> podciągnięte do 5 V, a **ESP32-S3 ma piny I2C tylko 3,3 V** (nie są
> 5 V-tolerancyjne), do tego ADS1115 przy 5 V uznaje HIGH dopiero od ~3,5 V.
> W praktyce u wielu osób działa to bezpośrednio (progi realnie są niższe),
> ale to poza specyfikacją. **Jeśli w logu zobaczysz `ADS1115: brak odpowiedzi`
> albo odczyty skaczą — dołóż 2-kanałowy konwerter poziomów I2C (BSS138, ~2 zł)
> między ESP32 a ADS1115.** Reszta połączeń zostaje bez zmian.

---

## 3. ADS1115 (I2C 0x48)

| Pin | Podłącz do |
|---|---|
| VCC (na płytce; w datasheecie **VDD**) | **5 V** (wymagane) |
| GND | GND |
| SDA | GPIO 8 |
| SCL | GPIO 9 |
| ADDR | GND (adres 0x48) |
| AIN0 | **A0 z MiCS wprost** (patrz wyżej) |
| AIN1 | wolne |
| AIN2 | MQ-7 (A0) |
| AIN3 | wolne |

---

## 4. Waveshare TVOC (SKU 30712) — UART 115200

| Pin | Podłącz do | Uwaga |
|---|---|---|
| VCC | 5 V | |
| GND | GND | |
| TXD | **GPIO 14** (RX ESP32) | dane CO₂/CH₂O/TVOC |
| RXD | GPIO 15 (opcjonalnie) | komendy trybu query |

---

## 5. Karta microSD (moduł SPI 3,3 V)

| Pin | Podłącz do |
|---|---|
| VCC | 3,3 V (moduł **z 3,3 V**, nie 5 V!) |
| GND | GND |
| CS | GPIO 2 |
| SCK | GPIO 4 |
| MOSI | GPIO 5 |
| MISO | GPIO 6 |

---

## 6. Pasek LED RGBIC (WS2812B, 5 V)

| Element | Podłączenie |
|---|---|
| DIN paska | 330 Ω → **GPIO 7** (przez level-shifter 74AHCT125 3,3→5 V) |
| 5 V paska | **osobny zasilacz 5 V ≥ 4 A** |
| GND paska | wspólna masa z płytką i zasilaczami |
| kondensator | 1000 µF między 5 V a GND przy pasku |

---

## 7. Ruch lokalny (PIR HC-SR505 / SR505, 3 piny)

| Pin | Podłącz do | Uwaga |
|---|---|---|
| VCC | **5 V** | SR505 działa z 4,5–20 V |
| GND | GND | |
| OUT | **GPIO 21** | wyjście cyfrowe 3,3 V — wprost do ESP32 |

Po wykryciu ruchu OUT idzie na HIGH na ~8 s; firmware rozjaśnia pasek LED na 60 s.
Po włączeniu SR505 ma ~30–60 s rozgrzewki (możliwy fałszywy impuls).

---

## 8. Zasilanie — układ

```
zasilacz 5 V ≥ 3 A  ── 5 V ──┬─ ESP32-S3 (VIN/5V)
  (obudowa mastera)          ├─ PMS7003 (VCC 1,2)
                             ├─ TVOC 30712 (VCC)
                             ├─ MiCS5524 (VCC)
                             └─ (opcjonalnie buck 5→3,3 V 2 A dla czujników 3V3)

GND wspólny dla WSZYSTKIEGO (płytka, czujniki, zasilacze, pasek LED).

pasek LED: osobny zasilacz 5 V ≥ 4 A, masa wspólna.
```

Zasada: gwiazda masy przy zasilaczu, krótkie i grube przewody zasilania (0,5 mm²)
do czujników prądożernych (PMS7003, pasek), cienkie sygnałowe skręcone parami.

---

## 9. Legenda przewodów (sugestia, trzymaj w całym projekcie)

| Linia | Kolor |
|---|---|
| 5 V | czerwony |
| 3,3 V | pomarańczowy |
| GND | czarny |
| SDA | zielony |
| SCL | niebieski |
| UART RX (dane do ESP) | żółty |
| UART TX (z ESP) | biały |
| dane LED | fioletowy |

---

## 10. Sprawdzenie po zlutowaniu

1. **Przed włączeniem:** miernikiem sprawdź brak zwarć 5 V ↔ GND i 3,3 V ↔ GND.
2. Włącz i obserwuj log na COM13 (115200):
   - `PMS7003: UART2 ...` i co 30 s `PMS: PM1.0=... PM2.5=... PM10=...`,
   - `TVOC: CO2=... ppm, CH2O=... ppb, TVOC=...`,
   - `ADS1115: OK (0x48 ...)` i `MiCS5524: EN=HIGH`,
   - brak `E ADS1115: brak odpowiedzi`.
3. Strona http://192.168.1.141 → Pulpit: PM, gazy i MiCS pokazują wartości.
4. Kalibracja MiCS (czyste powietrze, po 15 min): zakładka **Kalibracja** → „Zapisz R0".

> EMI przy drukarce: silniki i grzałki sieją zakłócenia — trzymaj analogowe
> (MiCS, ADS1115) z dala od kabli silników, sygnały prowadź skrętką, masę łącz
> w gwiazdę przy zasilaczu.
