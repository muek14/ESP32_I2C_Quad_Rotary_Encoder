// ============================================================================
//  ESP32_I2C_Quad_Rotary_Encoder - Testprogramm fuer das
//  Adafruit I2C Quad Rotary Encoder Breakout (PID 5752, seesaw/ATtiny817)
//  mit Anzeige auf dem GMT130 Display (ST7789, 240x240).
//
//  Board:   ELEGOO ESP32 (30 Pins, ESP-WROOM-32)
//  Library: Adafruit seesaw, TFT_eSPI (Konfiguration in platformio.ini)
//
//  Verdrahtung Encoder-Modul (STEMMA-QT-Kabel):
//    Gelb   SCL  -> D22
//    Blau   SDA  -> D21
//    Rot    V+   -> 3V3   (nicht 5V!)
//    Schwarz GND -> GND
//
//  Verdrahtung Display: wie im Projekt ESP32_TFT_GMT130
//    SCL D18, SDA D23, RES D26, DC D27, BLK D25, VCC 3V3, GND GND
//
//  Funktion:
//    - I2C-Scan beim Start (seriell + Display)
//    - Position aller 4 Drehgeber als Zahl und als Zeiger-Skala
//    - Tasterzustand jedes Drehgebers
//    - NeoPixel-Farbe folgt der Drehung, beim Druecken leuchtet sie weiss
//    - Taster > 1 s gedrueckt halten setzt den Drehgeber auf 0 zurueck
// ============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <Adafruit_seesaw.h>
#include <seesaw_neopixel.h>

// --- I2C / seesaw -----------------------------------------------------------
static const uint8_t  I2C_SDA      = 21;
static const uint8_t  I2C_SCL      = 22;
static const uint8_t  SEESAW_ADDR  = 0x49;    // Werkseinstellung (Bruecken A0..A2 geschlossen)
static const uint16_t PRODUCT_ID   = 5752;

// seesaw-interne Pinnummern (aus dem Adafruit-Beispiel PID5752_QuadEncoder_demo)
static const uint8_t SS_NEO_PIN = 18;
static const uint8_t SWITCH_PINS[4] = {12, 14, 17, 9};

static const uint32_t LONG_PRESS_MS = 1000;   // Reset der Position

Adafruit_seesaw ss(&Wire);
seesaw_NeoPixel pixels(4, SS_NEO_PIN, NEO_GRB + NEO_KHZ800);

// --- Display ----------------------------------------------------------------
TFT_eSPI tft = TFT_eSPI();
static const bool INVERT_COLORS = true;       // wie im Projekt ESP32_TFT_GMT130

static const int HEADER_H = 26;
static const int ROW_H    = 53;               // 4 Zeilen * 53 = 212 + 26 = 238
static const int DIAL_R   = 21;

// --- Zustand ----------------------------------------------------------------
struct EncoderState {
  int32_t  position   = 0;                    // angezeigte Position = Rohwert - offset
  int32_t  offset     = 0;                    // Nullpunkt (per langem Druck gesetzt)
  bool     pressed    = false;
  uint32_t pressStart = 0;
  bool     resetDone  = false;
  int32_t  shownPos   = INT32_MIN;            // zuletzt gezeichneter Wert
  int      shownPress = -1;
};
static EncoderState enc[4];

// ============================================================================
// Hilfsfunktionen
// ============================================================================

// Farbrad 0..255 -> RGB (wie im Adafruit-Beispiel)
static void wheelRGB(uint8_t pos, uint8_t &r, uint8_t &g, uint8_t &b)
{
  pos = 255 - pos;
  if (pos < 85)       { r = 255 - pos * 3; g = 0;             b = pos * 3; }
  else if (pos < 170) { pos -= 85;  r = 0;             g = pos * 3;       b = 255 - pos * 3; }
  else                { pos -= 170; r = pos * 3;       g = 255 - pos * 3; b = 0; }
}

static uint8_t wheelIndex(int32_t position)
{
  return (uint8_t)((position * 8) & 0xFF);    // *8 -> schnellere Farbaenderung
}

static void updatePixel(int e)
{
  uint8_t r, g, b;
  if (enc[e].pressed) { r = g = b = 255; }
  else                { wheelRGB(wheelIndex(enc[e].position), r, g, b); }
  pixels.setPixelColor(e, pixels.Color(r, g, b));
}

// ============================================================================
// Display
// ============================================================================

static void drawHeader(const char *text, uint16_t bg)
{
  tft.fillRect(0, 0, tft.width(), HEADER_H, bg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, bg);
  tft.drawString(text, tft.width() / 2, HEADER_H / 2, 2);
}

static void drawStaticLayout()
{
  tft.fillScreen(TFT_BLACK);
  drawHeader("Quad Rotary Encoder 0x49", TFT_NAVY);

  for (int e = 0; e < 4; e++) {
    int y0 = HEADER_H + e * ROW_H;
    if (e > 0) tft.drawFastHLine(0, y0, tft.width(), TFT_DARKGREY);

    // Skala: 24 Teilstriche (PEC11-Drehgeber haben 24 Rastungen)
    int cx = 30, cy = y0 + ROW_H / 2;
    tft.drawCircle(cx, cy, DIAL_R + 2, TFT_DARKGREY);
    for (int i = 0; i < 24; i++) {
      float a = i * (2 * PI / 24) - PI / 2;
      int x1 = cx + cosf(a) * (DIAL_R + 2), y1 = cy + sinf(a) * (DIAL_R + 2);
      int x2 = cx + cosf(a) * (DIAL_R - 1), y2 = cy + sinf(a) * (DIAL_R - 1);
      tft.drawLine(x1, y1, x2, y2, TFT_DARKGREY);
    }

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    char label[8];
    snprintf(label, sizeof(label), "Enc #%d", e);
    tft.drawString(label, 64, y0 + 5, 2);
  }
}

static void drawEncoder(int e)
{
  EncoderState &s = enc[e];
  int y0 = HEADER_H + e * ROW_H;
  int cx = 30, cy = y0 + ROW_H / 2;

  uint8_t r, g, b;
  wheelRGB(wheelIndex(s.position), r, g, b);
  uint16_t color = tft.color565(r, g, b);

  if (s.position != s.shownPos) {
    // Zeiger: alten Inhalt loeschen, neuen Zeiger zeichnen
    tft.fillCircle(cx, cy, DIAL_R - 2, TFT_BLACK);
    float a = (s.position % 24) * (2 * PI / 24) - PI / 2;
    tft.drawWideLine(cx, cy, cx + cosf(a) * (DIAL_R - 4), cy + sinf(a) * (DIAL_R - 4),
                     3, color, TFT_BLACK);
    tft.fillCircle(cx, cy, 3, color);

    // Zahlenwert
    char buf[12];
    snprintf(buf, sizeof(buf), "%ld", (long)s.position);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(color, TFT_BLACK);
    tft.setTextPadding(110);
    tft.drawString(buf, 64, y0 + 22, 4);
    tft.setTextPadding(0);

    s.shownPos = s.position;
  }

  if ((int)s.pressed != s.shownPress) {
    int bx = 180, by = y0 + 13, bw = 54, bh = 27;
    if (s.pressed) {
      tft.fillRoundRect(bx, by, bw, bh, 6, TFT_RED);
      tft.setTextColor(TFT_WHITE, TFT_RED);
    } else {
      tft.fillRoundRect(bx, by, bw, bh, 6, TFT_BLACK);
      tft.drawRoundRect(bx, by, bw, bh, 6, TFT_DARKGREY);
      tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    }
    tft.setTextDatum(MC_DATUM);
    tft.drawString("PUSH", bx + bw / 2, by + bh / 2 + 1, 2);
    s.shownPress = s.pressed;
  }
}

// Fehlermeldung anzeigen und anhalten
static void fatal(const char *line1, const char *line2)
{
  Serial.printf("FEHLER: %s %s\n", line1, line2);
  tft.fillScreen(TFT_BLACK);
  drawHeader("FEHLER", TFT_RED);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(line1, tft.width() / 2, 100, 2);
  tft.drawString(line2, tft.width() / 2, 125, 2);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("Verdrahtung pruefen:", tft.width() / 2, 165, 2);
  tft.drawString("SDA=D21 SCL=D22 V+=3V3", tft.width() / 2, 185, 2);
  while (true) delay(100);
}

// ============================================================================
// I2C-Scan
// ============================================================================

static int i2cScan()
{
  Serial.println("I2C-Scan (SDA=21, SCL=22):");
  tft.fillScreen(TFT_BLACK);
  drawHeader("I2C-Scan", TFT_NAVY);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  int found = 0, y = HEADER_H + 10;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      char buf[40];
      snprintf(buf, sizeof(buf), "Geraet bei 0x%02X%s", addr,
               addr == SEESAW_ADDR ? "  (Encoder)" : "");
      Serial.printf("  %s\n", buf);
      tft.drawString(buf, 10, y, 2);
      y += 20;
      found++;
    }
  }
  if (found == 0) {
    Serial.println("  keine Geraete gefunden");
    tft.drawString("keine Geraete gefunden", 10, y, 2);
  }
  delay(1500);
  return found;
}

// ============================================================================

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("=== ESP32 + Adafruit Quad Rotary Encoder (5752) Test ===");

  tft.init();
  tft.setRotation(0);
  tft.invertDisplay(INVERT_COLORS);

  Wire.begin(I2C_SDA, I2C_SCL);
  i2cScan();

  if (!ss.begin(SEESAW_ADDR) || !pixels.begin(SEESAW_ADDR)) {
    fatal("seesaw nicht gefunden", "Adresse 0x49");
  }
  uint16_t pid = (ss.getVersion() >> 16) & 0xFFFF;
  Serial.printf("seesaw gefunden, Produkt-ID: %u\n", pid);
  if (pid != PRODUCT_ID) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Produkt-ID %u statt 5752", pid);
    fatal("Falsche Firmware?", buf);
  }

  uint32_t mask = 0;
  for (int e = 0; e < 4; e++) {
    ss.pinMode(SWITCH_PINS[e], INPUT_PULLUP);
    mask |= 1UL << SWITCH_PINS[e];
    enc[e].position = ss.getEncoderPosition(e);
  }

  pixels.setBrightness(60);
  for (int e = 0; e < 4; e++) updatePixel(e);
  pixels.show();

  drawStaticLayout();
  for (int e = 0; e < 4; e++) drawEncoder(e);

  Serial.println("Bereit - Drehgeber drehen / druecken (lange druecken = Reset auf 0)");
}

void loop()
{
  // Alle 4 Taster in einem einzigen I2C-Zugriff lesen
  uint32_t mask = 0;
  for (int e = 0; e < 4; e++) mask |= 1UL << SWITCH_PINS[e];
  uint32_t buttons = ss.digitalReadBulk(mask);

  bool pixelsChanged = false;
  uint32_t now = millis();

  for (int e = 0; e < 4; e++) {
    EncoderState &s = enc[e];

    int32_t pos = ss.getEncoderPosition(e) - s.offset;
    if (pos != s.position) {
      Serial.printf("Enc #%d -> %ld\n", e, (long)pos);
      s.position = pos;
      pixelsChanged = true;
    }

    bool pressed = !(buttons & (1UL << SWITCH_PINS[e]));   // aktiv LOW
    if (pressed != s.pressed) {
      s.pressed = pressed;
      Serial.printf("Enc #%d %s\n", e, pressed ? "gedrueckt" : "losgelassen");
      if (pressed) { s.pressStart = now; s.resetDone = false; }
      pixelsChanged = true;
    }

    // Langer Druck -> Position auf 0.
    // Der Nullpunkt wird hier im ESP32 verschoben: ss.setEncoderPosition()
    // wird von der Firmware des Moduls nicht dauerhaft uebernommen
    // (die Anzeige sprang nach dem Reset wieder auf den alten Wert).
    if (s.pressed && !s.resetDone && now - s.pressStart >= LONG_PRESS_MS) {
      s.offset  += s.position;
      s.position = 0;
      s.resetDone = true;
      Serial.printf("Enc #%d auf 0 zurueckgesetzt\n", e);
      pixelsChanged = true;
    }

    if (pixelsChanged) updatePixel(e);
    drawEncoder(e);   // zeichnet nur, wenn sich etwas geaendert hat
  }

  if (pixelsChanged) pixels.show();
  delay(10);
}
