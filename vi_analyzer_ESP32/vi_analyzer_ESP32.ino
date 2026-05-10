/*
===============================================================================
Name:         vi_analyzer_ESP32
Version:      1.0.0
Author:       Alejandro Alonso Puig (https://github.com/aalonsopuig) + GPT
Date:         2026-05
License:      Apache 2.0
-------------------------------------------------------------------------------
Description:

Simple V/I curve analyzer proof of concept for ESP32 + SSD1306 OLED.

Main features:
- Reads two analog channels:
    * Voltage axis  (X)
    * Current axis (Y)
- Displays an X/Y graph similar to oscilloscope XY mode.
- Uses centered bipolar mapping.
- Automatic zero calibration at startup.
- Dotted axes for better curve visibility.
- Information area on right side.

Hardware:
- ESP32 classic
- SSD1306 OLED 128x64 I2C
- External analog conditioning circuitry

===============================================================================
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define OLED_ADDR     0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const int PIN_V = 34;
const int PIN_I = 35;

// Logical display scale: -4V ... +4V
// Physical input range is approximately -3.3V ... +3.3V after conditioning.
const float V_FULL_SCALE = 4.0;

// Series resistor used for current measurement
const float R_SHUNT = 330.0;

const int GRID_X = 0;
const int GRID_Y = 0;
const int GRID_SIZE = 63;
const int DIVS = 10;
const int TICK = 2;

const int CX = GRID_X + GRID_SIZE / 2;
const int CY = GRID_Y + GRID_SIZE / 2;

const int TEXT_X = 68;

const int N = 96;

int xPoints[N];
int yPoints[N];

int zeroV = 2048;
int zeroI = 2048;

void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (true);
  }

  showSplash();
  calibrateZero();
}

void loop() {
  captureSamples();

  display.clearDisplay();

  drawGrid();
  drawAxesDots();
  drawCurve();
  drawInfo();

  display.display();

  delay(50);
}

void showSplash() {
  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);

  display.setCursor(0, 12);
  display.println("V/I");
  display.println("analyzer");

  display.setTextSize(1);
  display.setCursor(0, 52);
  display.println("v1.0");

  display.display();

  delay(2000);
}

void calibrateZero() {
  long sumV = 0;
  long sumI = 0;

  const int samples = 200;

  for (int n = 0; n < samples; n++) {
    sumV += analogRead(PIN_V);
    sumI += analogRead(PIN_I);
    delay(2);
  }

  zeroV = sumV / samples;
  zeroI = sumI / samples;
}

void captureSamples() {
  for (int n = 0; n < N; n++) {
    int adcV = analogRead(PIN_V);
    int adcI = analogRead(PIN_I);

    xPoints[n] = mapCenteredX(adcV, zeroV);
    yPoints[n] = mapCenteredYInverted(adcI, zeroI);
  }
}

int mapCenteredX(int adc, int zero) {
  int delta = adc - zero;

  // ADC physical span after conditioning:
  // 0..4095 represents approximately -3.3V..+3.3V.
  // We still display the graph with a logical scale of -4V..+4V.
  int x = CX + map(delta, -2048, 2047, -GRID_SIZE / 2, GRID_SIZE / 2);

  return constrain(x, GRID_X, GRID_X + GRID_SIZE);
}

int mapCenteredYInverted(int adc, int zero) {
  int delta = adc - zero;

  // Current channel intentionally inverted.
  int y = CY + map(delta, -2048, 2047, -GRID_SIZE / 2, GRID_SIZE / 2);

  return constrain(y, GRID_Y, GRID_Y + GRID_SIZE);
}

void drawGrid() {
  display.drawRect(GRID_X, GRID_Y, GRID_SIZE, GRID_SIZE, SSD1306_WHITE);

  int step = GRID_SIZE / DIVS;

  for (int i = 1; i < DIVS; i++) {
    int x = GRID_X + i * step;
    int y = GRID_Y + i * step;

    display.drawLine(x, GRID_Y, x, GRID_Y + TICK, SSD1306_WHITE);
    display.drawLine(x, GRID_Y + GRID_SIZE, x, GRID_Y + GRID_SIZE - TICK, SSD1306_WHITE);

    display.drawLine(GRID_X, y, GRID_X + TICK, y, SSD1306_WHITE);
    display.drawLine(GRID_X + GRID_SIZE, y, GRID_X + GRID_SIZE - TICK, y, SSD1306_WHITE);
  }
}

void drawAxesDots() {
  for (int x = GRID_X; x <= GRID_X + GRID_SIZE; x += 4) {
    display.drawPixel(x, CY, SSD1306_WHITE);
  }

  for (int y = GRID_Y; y <= GRID_Y + GRID_SIZE; y += 4) {
    display.drawPixel(CX, y, SSD1306_WHITE);
  }
}

void drawCurve() {
  for (int n = 1; n < N; n++) {
    display.drawLine(
      xPoints[n - 1],
      yPoints[n - 1],
      xPoints[n],
      yPoints[n],
      SSD1306_WHITE
    );
  }
}

void drawInfo() {
  float voltsPerDiv = (2.0 * V_FULL_SCALE) / DIVS;
  float mAperDiv = (voltsPerDiv / R_SHUNT) * 1000.0;

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(TEXT_X, 0);
  display.println("V/I graph");

  display.setCursor(TEXT_X, 16);
  display.print("V: ");
  display.print(voltsPerDiv, 1);
  display.println("v");

  display.setCursor(TEXT_X, 26);
  display.print("I: ");
  display.print(mAperDiv, 1);
  display.println("mA");

  display.setCursor(TEXT_X, 36);
  display.print("R: ");
  display.print((int)R_SHUNT);
}
