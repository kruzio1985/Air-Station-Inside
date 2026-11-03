# FilamentNode — instrukcja lutowania i połączeń (węzeł ESP32-C6, Etap 2)

Węzeł C6 zbiera wilgotność i temperaturę z **8 czujników AHT20** (po jednym
w każdym pojemniku na filament) i wysyła je przez **Zigbee → Zigbee2MQTT →
MQTT → master** (FilamentMonitor na ESP32-S3). Dane idą po Zigbee, ale węzeł
ma też **własną prostą stronę www przez WiFi** (AP `FilamentNode-XXXX`) do
ustawienia WiFi, nazwy węzła i podglądu odczytów. Opisy pojemników (producent,
rodzaj, kolor, wilgotność docelowa) są na masterze — tam są wyświetlane.

Firmware: [C:\Projects\FilamentNode\src\main.cpp](../FilamentNode/src/main.cpp)
Piny I2C na C6: **SDA = GPIO 6**, **SCL = GPIO 7**.

---

## Spis elementów (do zlutowania)

| Element | Ilość | Uwaga |
|---|---|---|
| ESP32-C6 devkit (z USB-C) | 1 | płytka węzła |
| TCA9548A (moduł multipleksera I2C) | 1 | adres 0x70 (A0=A1=A2=GND) |
| AHT20 (moduł z goldpinami, fioletowa płytka) | 8 | wszystkie mają adres 0x38 |
| przewody AWG24–28 (do I2C najlepiej skrętka 2-żyłowa) | ok. 1–2 m | |
| opcjonalnie: koszulki termokurczliwe, taśma 4-żyłowa | | |

**Po co TCA9548A:** wszystkie AHT20 mają **ten sam adres I2C (0x38)** — nie da
się podłączyć 8 sztuk na jedną magistralę. Multiplekser TCA9548A tworzy
**8 osobnych kanałów** i firmware przełącza je po kolei (kanał 0 → pojemnik 1,
kanał 7 → pojemnik 8).

---

## 1. Zasilanie (wspólne dla wszystkiego: 3,3 V)

Wszystko zasilaj z **3,3 V** (nie z 5 V — czujniki i multiplekser są 3,3 V,
a C6 ma logikę 3,3 V):

- C6: zasilanie z **USB-C** (np. ładowarka 5 V/1 A) — na płytce jest stabilizator 3,3 V,
- pin **3V3** na C6 → szyna 3,3 V (VIN) dla TCA9548A i wszystkich AHT20,
- pin **GND** na C6 → wspólna szyna GND.

Pobór jest mały: 8× AHT20 to ok. **1–2 mA** w pomiarze, TCA9548A do ok. 5 mA.
Zasilanie przez USB 5 V w zupełności wystarcza.

> ⚠️ Wspólny GND to podstawa — wszystkie moduły muszą mieć **jeden wspólny
> minus**, inaczej I2C nie zadziała.

---

## 2. C6 → TCA9548A (magistrala główna I2C)

| TCA9548A | Podłącz do C6 | Uwaga |
|---|---|---|
| **VIN** | **3V3** | zasilanie multipleksera |
| **GND** | **GND** | wspólna masa |
| **SCL** | **GPIO 7** | zegar I2C |
| **SDA** | **GPIO 6** | dane I2C |
| **A0** | GND | adres I2C = 0x70 |
| **A1** | GND | jw. |
| **A2** | GND | jw. |
| **RST** (jeśli jest) | 3V3 | aktywny niski — trzymaj wysoko |

Piny na C6 znajdziesz na listwach goldpin. W większości devkitów C6 opisy
`6` i `7` są nadrukowane; jeśli nie — GPIO6/GPIO7 to piny na górnym rzędzie
(patrz nadruk silkscreen na spodzie płytki).

---

## 3. TCA9548A → 8× AHT20 (kanały 0–7)

Multiplekser ma po **2 piny na każdy kanał**: `SC0..SC7` (zegar kanału)
i `SD0..SD7` (dane kanału). Do każdego kanału podpinasz **jeden** AHT20.

Moduł AHT20 ma zwykle 4 goldpiny: **VIN, GND, SCL, SDA**.

| Pojemnik | Kanał TCA | SDA AHT20 | SCL AHT20 | VIN / GND |
|---|---|---|---|---|
| 1 | 0 | **SD0** | **SC0** | 3V3 / GND |
| 2 | 1 | **SD1** | **SC1** | 3V3 / GND |
| 3 | 2 | **SD2** | **SC2** | 3V3 / GND |
| 4 | 3 | **SD3** | **SC3** | 3V3 / GND |
| 5 | 4 | **SD4** | **SC4** | 3V3 / GND |
| 6 | 5 | **SD5** | **SC5** | 3V3 / GND |
| 7 | 6 | **SD6** | **SC6** | 3V3 / GND |
| 8 | 7 | **SD7** | **SC7** | 3V3 / GND |

Czyli na przykład **pojemnik 1**: AHT20.SDA → TCA `SD0`, AHT20.SCL → TCA `SC0`,
AHT20.VIN → 3V3, AHT20.GND → GND. Pojemnik 2: `SD1`/`SC1` itd.

> 💡 **NIE łącz** SDA/SCL wszystkich AHT20 razem i do jednego pinu C6 —
> to byłby zwykły I2C z 8 czujnikami o tym samym adresie i **nic by nie
> działało**. Każdy AHT20 idzie na **swój własny kanał** TCA.

---

## 4. Przewody do pojemników (ważne — I2C na dystans)

AHT20 siedzą w pojemnikach na filament, więc kable muszą mieć trochę długości:

- użyj **skrętki 2-żyłowej** (np. żyła z kabla sieciowego CAT5/6): jedna żyła
  = SDA, druga = SCL; **VIN i GND poprowadź osobnymi żyłami** (najlepiej
  grubszymi, AWG24), żeby napięcie nie siadło,
- **długość kabla do ok. 1–2 m** na kanał jest bezpieczna; przy większych
  odległościach I2C robi się zawodne — wtedy obniż prędkość magistrali albo
  skróć kable,
- moduły AHT20 zwykle **mają już rezystory podciągające** (10 kΩ na SDA/SCL) —
  to jest OK, bo w danym momencie aktywny jest tylko jeden kanał TCA,
- nie prowadź kabli I2C tuż obok kabli silników drukarki / zasilaczy (zakłócenia).

---

## 5. Kolejność lutowania (bezpieczna)

1. **Najpierw GND i 3V3** na TCA9548A i na wszystkich AHT20 (wspólna szyna).
2. **C6 → TCA**: SDA (6) → SDA, SCL (7) → SCL.
3. **TCA → AHT20**: po kolei kanały 0–7 (SDA kanału → SDA czujnika,
   SCL kanału → SCL czujnika).
4. Sprawdź **miernikiem (ciągłość)**: brak zwarcia 3V3 ↔ GND przed włączeniem.
5. Podłącz USB-C do C6.

Temperatura grota **320–330 °C**, cienka cyna z topnikiem, czas styku 2–3 s na pin.
Po zlutowaniu sprawdź lupą, czy nie ma mostków cyny między sąsiednimi pinami.

---

## 6. Test po zlutowaniu

Po wgraniu firmware ([platformio.ini](../FilamentNode/platformio.ini),
`pio run -t upload`) otwórz port szeregowy C6 (USB-C, 115200 Bd).
Prawidłowy start wygląda tak:

```
FilamentNode - wezel Zigbee (8x AHT20 przez TCA9548A)
Chip: ESP32-C6, Flash: 16 MB
Pojemnik 1 (kanal TCA 0): AHT20 OK
Pojemnik 2 (kanal TCA 1): AHT20 OK
... (itd. — dla podłączonych czujników)
Start Zigbee (end device)...
Zigbee wystartowal. Czekam na siec (Z2M: Permit Join).
```

- `BRAK czujnika` na kanale = AHT20 nie podłączony / źle zlutowany / zły kanał.
- Po ~5 s w logu pojawią się odczyty: `Pojemnik 1: 21.34 C, 44.12 %`.
- Jak wszystkie 8 pokazują `AHT20 OK` i odczyty, węzeł jest gotowy do
  sparowania w Zigbee2MQTT (Permit Join → dodaj urządzenie `FilamentNode`).

---

## 6a. Strona www węzła (konfiguracja przez WiFi)

Węzeł po starcie uruchamia **punkt dostępowy WiFi**: `FilamentNode-XXXX`
(hasło AP), adres **http://192.168.4.1**. Strona pozwala:

- ustawić **SSID i hasło domowego WiFi** (wtedy węzeł łączy się też do sieci
  domowej — po adresie IP z DHCP, wygodniej do OTA),
- ustawić **nazwę węzła** (host OTA),
- zobaczyć **status Zigbee** i **live odczyty z 8 pojemników**.

API: `GET /api/status` zwraca JSON (node, ap_ssid, sta, ip, zigbee, boxes).

> AP działa **zawsze** (nawet jak WiFi domowe jest ustawione) — to zapasowe
> wejście do konfiguracji. Dane z czujników i tak idą po Zigbee, niezależnie
> od WiFi. MQTT i opisy pojemników zostają na masterze (węzeł Zigbee ich nie
> potrzebuje — bridge robi Zigbee2MQTT).

---

## 7. Wgrywanie firmware (Windows — ważna uwaga)

Na Windowsie `pio run -t upload` potrafi się **zawiesić** przy postępie
wgrania, bo Python PlatformIO próbuje wypisać znaki Unicode (pasek postępu
esptool) w kodowaniu cp1250 i wywala się w tle. Rozwiązanie — ustaw
kodowanie UTF-8 **przed** uploadem:

```powershell
$env:PYTHONIOENCODING = "utf-8"
$env:PYTHONUTF8 = "1"
cd C:\Projects\FilamentNode
pio run -e esp32c6 -t upload --upload-port COM22
```

Jeśli płytka po nieudanym wgraniu „nie odpowiada" (brak sygnału na porcie),
wykonaj **ręczny reset do bootloadera**: trzymaj przycisk **BOOT**, naciśnij
i puść **RST**, potem puść BOOT — i ponów upload. (Alternatywnie przez
USB-Serial/JTAG: toggle DTR/RTS.)
