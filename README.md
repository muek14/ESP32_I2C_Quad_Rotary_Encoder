# ESP32_I2C_Quad_Rotary_Encoder

Testprogramm für das **Adafruit I2C Quad Rotary Encoder Breakout with NeoPixel**
(Produkt 5752) an einem **ELEGOO ESP32** (30 Pins). Die Werte werden auf dem
**1.3" TFT-Display GMT130** (ST7789, 240×240) angezeigt.

![Plattform](https://img.shields.io/badge/Plattform-PlatformIO-orange)
![Framework](https://img.shields.io/badge/Framework-Arduino-blue)
![Library](https://img.shields.io/badge/Library-seesaw%20%7C%20TFT__eSPI-green)

---

## Hardware

| Komponente | Beschreibung |
|---|---|
| Mikrocontroller | ELEGOO ESP32 Board, 30 Pins (15 pro Seite), ESP-WROOM-32 |
| Encoder-Modul | [Adafruit 5752](https://www.adafruit.com/product/5752) – 4 Drehgeber mit Taster + 4 NeoPixel, seesaw-Firmware (ATtiny817), I²C |
| Drehgeber | 4 × PEC11-kompatibel (24 Rastungen), in die Plätze #0–#3 eingelötet |
| Display | Golden Morning [GMT130 V1.0](https://goldenmorninglcd.com/de/TFT-Anzeigemodul/1.3-Zoll-240-x-240-st7789-gmt130-v1.0/), 1.3", 240×240, ST7789, SPI ohne CS |

### Verdrahtung Encoder-Modul (STEMMA-QT-Kabel)

| Kabelfarbe | Signal | ESP32 |
|---|---|---|
| Gelb | SCL | D22 |
| Blau | SDA | D21 |
| Rot | V+ | **3V3** (nicht 5V/VIN!) |
| Schwarz | GND | GND |

Die 10-kΩ-Pull-ups des Moduls ziehen SDA/SCL auf die Versorgungsspannung –
deshalb muss das Modul mit 3,3 V versorgt werden. Der INT-Pin wird nicht
verwendet (das STEMMA-QT-Kabel hat nur 4 Adern), das Modul wird alle 10 ms abgefragt.

I²C-Adresse: **0x49** (Werkseinstellung, Lötbrücken A0–A2 geschlossen).

### Verdrahtung Display

| Display-Pin | Funktion | ESP32 |
|---|---|---|
| GND | Masse | GND |
| VCC | Versorgung | 3V3 |
| SCL | SPI-Takt (SCK) | D18 |
| SDA | SPI-Daten (MOSI) | D23 |
| RES | Reset | D26 |
| DC | Daten/Befehl | D27 |
| BLK | Hintergrundbeleuchtung | D25 |

```
                  ELEGOO ESP32 (30 Pins)
                   ┌─────────────────────┐
                EN │●                   ●│ D23  ── SDA   (Display)
                VP │●                   ●│ D22  ── SCL   (Encoder, gelb)
                VN │●                   ●│ TX0
               D34 │●                   ●│ RX0
               D35 │●                   ●│ D21  ── SDA   (Encoder, blau)
               D32 │●                   ●│ D19
               D33 │●                   ●│ D18  ── SCL   (Display)
    Disp BLK ─ D25 │●                   ●│ D5
    Disp RES ─ D26 │●                   ●│ TX2
    Disp DC  ─ D27 │●                   ●│ RX2
               D14 │●                   ●│ D4
               D12 │●                   ●│ D2
               D13 │●                   ●│ D15
               GND │●                   ●│ GND  ── GND   (Display + Encoder)
               VIN │●                   ●│ 3V3  ── VCC/V+ (Display + Encoder)
                   └────────[USB]────────┘
```

---

## Software

- [Visual Studio Code](https://code.visualstudio.com/) mit [PlatformIO IDE](https://platformio.org/)
- Plattform `espressif32`, Framework `arduino`
- Bibliotheken (werden automatisch über `lib_deps` installiert):
  - [Adafruit seesaw Library](https://github.com/adafruit/Adafruit_Seesaw)
  - [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI)

Das Display wird – wie im Projekt `ESP32_TFT_GMT130` – vollständig über die
`build_flags` in der [platformio.ini](platformio.ini) konfiguriert
(ST7789, 240×240, `TFT_CS=-1`, **`TFT_SPI_MODE=SPI_MODE3`**, 40 MHz).

---

## Funktion

**Beim Start**
1. I²C-Scan – gefundene Adressen werden seriell und auf dem Display angezeigt
2. Prüfung, ob unter 0x49 das seesaw-Modul mit Produkt-ID **5752** antwortet –
   sonst rote Fehlermeldung mit Verdrahtungshinweis

**Im Betrieb** – pro Drehgeber eine Zeile auf dem Display:
- runde Skala mit 24 Teilstrichen und mitdrehendem Zeiger
- Positionswert als Zahl (in der Farbe der zugehörigen NeoPixel-LED)
- „PUSH“-Feld, das beim Drücken rot wird

**NeoPixel**: Farbe wandert beim Drehen durch den Regenbogen, beim Drücken weiß.

**Reset**: Taster **länger als 1 s** halten → Drehgeber wird auf 0 gesetzt.

Serielle Ausgabe (115200 Baud):

```
=== ESP32 + Adafruit Quad Rotary Encoder (5752) Test ===
I2C-Scan (SDA=21, SCL=22):
  Geraet bei 0x49  (Encoder)
seesaw gefunden, Produkt-ID: 5752
Bereit - Drehgeber drehen / druecken (lange druecken = Reset auf 0)
Enc #0 -> 1
Enc #0 gedrueckt
Enc #0 auf 0 zurueckgesetzt
```

### Hinweis zum Reset

`ss.setEncoderPosition()` wird von der Firmware des Moduls nicht dauerhaft
übernommen – die Anzeige sprang danach wieder auf den alten Wert. Der Reset
wird deshalb im ESP32 über einen Nullpunkt (`offset`) pro Drehgeber gelöst:

```
angezeigter Wert = Zählerstand des Moduls − offset
```

Der Nullpunkt geht beim Neustart des ESP32 verloren.

---

## Bauen und Hochladen

In VS Code über die PlatformIO-Leiste **✓ Build**, **→ Upload**, **🔌 Monitor**
oder im Terminal:

```bash
pio run
pio run -t upload
pio device monitor
```

> In einem Workspace mit mehreren PlatformIO-Projekten baut der Build-Knopf das
> in der Statusleiste ausgewählte Projekt. Bei Fehlern wie
> „Starting directory (cwd) … does not exist“: **Developer: Reload Window**.

---

## Fehlersuche

| Problem | Lösung |
|---|---|
| I²C-Scan findet nichts | Kabel prüfen: Gelb→D22, Blau→D21, Rot→3V3, Schwarz→GND; grüne LED am Modul an? |
| Gerät gefunden, aber nicht bei 0x49 | Lötbrücken A0–A2 auf der Rückseite prüfen (A0 +1, A1 +2, A2 +4) |
| „Falsche Firmware?“ | Modul ist kein 5752 bzw. andere seesaw-Firmware |
| Drehrichtung falsch herum | Werte im Code negieren |
| NeoPixel zu hell/dunkel | `pixels.setBrightness(60)` anpassen (0–255) |
| Display schwarz | siehe Projekt `ESP32_TFT_GMT130` (SPI_MODE3, Verdrahtung) |

---

## Projektstruktur

```
ESP32_I2C_Quad_Rotary_Encoder/
├── include/
├── lib/
├── src/
│   └── ESP32_I2C_Quad_Rotary_Encoder.cpp   Testprogramm
├── test/
├── platformio.ini                          Projekt-, Bibliotheks- und Display-Konfiguration
├── LICENSE                                 MIT-Lizenz
└── README.md
```

---

## Siehe auch

- [ESP32_TFT_GMT130](https://github.com/muek14/ESP32_TFT_GMT130) – Testprogramm und Verdrahtung des GMT130-Displays

---

## Lizenz

Dieses Projekt steht unter der [MIT-Lizenz](LICENSE).
