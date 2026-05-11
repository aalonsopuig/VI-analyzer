/*
===============================================================================
Name:         vi_analyzer_arduino
Version:      1.0.0
Author:       Alejandro Alonso Puig (https://github.com/aalonsopuig) + GPT
Date:         2026-05
License:      Apache 2.0
-------------------------------------------------------------------------------
Description:

Simple V/I curve analyzer for Arduino Mega + TFT ILI9481.

===============================================================================
*/

#include <MCUFRIEND_kbv.h>
#include <Adafruit_GFX.h>

MCUFRIEND_kbv tft;

// -----------------------------------------------------------------------------
// Colors RGB565
// -----------------------------------------------------------------------------

#define BLACK   0x0000
#define WHITE   0xFFFF
#define GREEN   0x07E0
#define RED     0xF800
#define YELLOW  0xFFE0

// -----------------------------------------------------------------------------
// Analog inputs
// -----------------------------------------------------------------------------

const int PIN_V = A0;
const int PIN_I = A1;

// -----------------------------------------------------------------------------
// Measurement scaling
// -----------------------------------------------------------------------------

// Logical display scale: -4V ... +4V
const float V_FULL_SCALE = 4.0;

// Series resistor used for current measurement
const float R_SHUNT = 330.0;

// -----------------------------------------------------------------------------
// Screen and graph layout
// -----------------------------------------------------------------------------

const int SCREEN_W = 480;
const int SCREEN_H = 320;

// Left graph area
const int GRID_X = 10;
const int GRID_Y = 10;
const int GRID_SIZE = 300;

const int DIVS = 10;
const int TICK = 6;

const int CX = GRID_X + GRID_SIZE / 2;
const int CY = GRID_Y + GRID_SIZE / 2;

// Text area on right side
const int TEXT_X = 330;
const int TEXT_Y = 20;

// -----------------------------------------------------------------------------
// Sample buffers
// -----------------------------------------------------------------------------

const int N = 240;

int xPoints[N];
int yPoints[N];

// -----------------------------------------------------------------------------
// ADC zero calibration
// -----------------------------------------------------------------------------

int zeroV = 512;
int zeroI = 512;

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);

  uint16_t ID = tft.readID();
  tft.begin(ID);

  tft.setRotation(1);      // Landscape
  tft.fillScreen(BLACK);

  showSplash();
  calibrateZero();

  tft.fillScreen(BLACK);
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
  captureSamples();

  tft.fillRect(0, 0, 320, 320, BLACK);

  drawGrid();
  drawAxesDots();
  drawCurve();
  drawInfo();

  delay(50);
}

// =============================================================================
// SPLASH SCREEN
// =============================================================================

void showSplash() {
  tft.fillScreen(BLACK);

  tft.setTextColor(WHITE);
  tft.setTextSize(4);
  tft.setCursor(60, 100);
  tft.print("V/I analyzer");

  tft.setTextSize(2);
  tft.setCursor(190, 160);
  tft.print("v1.0");

  delay(2000);
}

// =============================================================================
// ZERO CALIBRATION
// =============================================================================

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

// =============================================================================
// SAMPLE ACQUISITION
// =============================================================================

void captureSamples() {
  for (int n = 0; n < N; n++) {
    int adcV = analogRead(PIN_V);
    int adcI = analogRead(PIN_I);

    xPoints[n] = mapCenteredX(adcV, zeroV);
    yPoints[n] = mapCenteredYInverted(adcI, zeroI);
  }
}

// =============================================================================
// ADC -> SCREEN MAPPING
// =============================================================================

int mapCenteredX(int adc, int zero) {
  int delta = adc - zero;

  int x = CX + map(
              delta,
              -512,
               511,
              -GRID_SIZE / 2,
               GRID_SIZE / 2
            );

  return constrain(x, GRID_X, GRID_X + GRID_SIZE);
}

int mapCenteredYInverted(int adc, int zero) {
  int delta = adc - zero;

  int y = CY + map(
              delta,
              -512,
               511,
              -GRID_SIZE / 2,
               GRID_SIZE / 2
            );

  return constrain(y, GRID_Y, GRID_Y + GRID_SIZE);
}

// =============================================================================
// GRID DRAWING
// =============================================================================

void drawGrid() {
  tft.drawRect(GRID_X, GRID_Y, GRID_SIZE, GRID_SIZE, GREEN);

  int step = GRID_SIZE / DIVS;

  for (int i = 1; i < DIVS; i++) {
    int x = GRID_X + i * step;
    int y = GRID_Y + i * step;

    tft.drawLine(x, GRID_Y, x, GRID_Y + TICK, GREEN);
    tft.drawLine(x, GRID_Y + GRID_SIZE, x, GRID_Y + GRID_SIZE - TICK, GREEN);

    tft.drawLine(GRID_X, y, GRID_X + TICK, y, GREEN);
    tft.drawLine(GRID_X + GRID_SIZE, y, GRID_X + GRID_SIZE - TICK, y, GREEN);
  }
}

// =============================================================================
// AXES DRAWING
// =============================================================================

void drawAxesDots() {
  for (int x = GRID_X; x <= GRID_X + GRID_SIZE; x += 8) {
    tft.drawPixel(x, CY, WHITE);
  }

  for (int y = GRID_Y; y <= GRID_Y + GRID_SIZE; y += 8) {
    tft.drawPixel(CX, y, WHITE);
  }
}

// =============================================================================
// CURVE DRAWING
// =============================================================================

void drawCurve() {
  for (int n = 1; n < N; n++) {
    tft.drawLine(
      xPoints[n - 1],
      yPoints[n - 1],
      xPoints[n],
      yPoints[n],
      RED
    );
  }
}

// =============================================================================
// INFORMATION AREA
// =============================================================================

void drawInfo() {
  float voltsPerDiv = (2.0 * V_FULL_SCALE) / DIVS;
  float mAperDiv = (voltsPerDiv / R_SHUNT) * 1000.0;

  tft.fillRect(320, 0, 160, 320, BLACK);

  tft.setTextColor(WHITE);
  tft.setTextSize(2);

  tft.setCursor(TEXT_X, TEXT_Y);
  tft.print("V/I graph");

  tft.setCursor(TEXT_X, TEXT_Y + 50);
  tft.print("V: ");
  tft.print(voltsPerDiv, 1);
  tft.print(" v");

  tft.setCursor(TEXT_X, TEXT_Y + 85);
  tft.print("I: ");
  tft.print(mAperDiv, 1);
  tft.print(" mA");

  tft.setCursor(TEXT_X, TEXT_Y + 120);
  tft.print("R: ");
  tft.print((int)R_SHUNT);
}
