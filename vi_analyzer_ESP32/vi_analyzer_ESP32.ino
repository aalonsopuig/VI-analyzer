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
- Uses centered bipolar mapping:
    * ADC midpoint = zero reference
- Automatic zero calibration at startup.
- 63x63 pixel square graph area.
- Dotted axes to improve curve visibility.
- Basic UI information area on the right side.

Hardware:
- ESP32 classic
- SSD1306 OLED 128x64 I2C
- Analog inputs shifted to 0..3.3V range externally

Connections:
OLED:
- SDA -> GPIO21
- SCL -> GPIO22
- VCC -> 3V3
- GND -> GND

Analog:
- Voltage input  -> GPIO34
- Current input  -> GPIO35

Notes:
- Inputs MUST remain within 0V..3.3V.
- External analog conditioning is required.
- Intended as educational / experimental PoC.

===============================================================================
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// -----------------------------------------------------------------------------
// OLED display configuration
// -----------------------------------------------------------------------------

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define OLED_ADDR     0x3C

// Create SSD1306 display object
Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

// -----------------------------------------------------------------------------
// ESP32 analog input pins
// ADC1 pins are preferred on ESP32
// -----------------------------------------------------------------------------

const int PIN_V = 34;      // Voltage channel
const int PIN_I = 35;      // Current channel

// -----------------------------------------------------------------------------
// Measurement scaling configuration
// -----------------------------------------------------------------------------

// Real external full scale:
// -5V ... +5V
//
// External conditioning converts it into:
// 0V ... 3.3V
//
const float V_FULL_SCALE = 5.0;

// Shunt resistor used for current calculation
const float R_SHUNT = 1000.0;

// -----------------------------------------------------------------------------
// Graph area configuration
// -----------------------------------------------------------------------------

// Graph located on left side of OLED
const int GRID_X = 0;
const int GRID_Y = 0;

// Square graph size
const int GRID_SIZE = 63;

// Number of divisions
const int DIVS = 10;

// Tick mark size
const int TICK = 2;

// Graph center coordinates
const int CX = GRID_X + GRID_SIZE / 2;
const int CY = GRID_Y + GRID_SIZE / 2;

// -----------------------------------------------------------------------------
// Text information area
// -----------------------------------------------------------------------------

const int TEXT_X = 68;

// -----------------------------------------------------------------------------
// Sample buffer configuration
// -----------------------------------------------------------------------------

// Number of captured points
const int N = 96;

// Arrays storing graph coordinates
int xPoints[N];
int yPoints[N];

// -----------------------------------------------------------------------------
// ADC zero calibration values
// -----------------------------------------------------------------------------

// Initialized near ADC midscale
int zeroV = 2048;
int zeroI = 2048;

// =============================================================================
// SETUP
// =============================================================================

void setup() {

  // Serial debug port
  Serial.begin(115200);

  // Configure ESP32 ADC resolution:
  // 12 bits -> 0..4095
  analogReadResolution(12);

  // 11dB attenuation:
  // allows reading approximately up to 3.3V
  analogSetAttenuation(ADC_11db);

  // Initialize I2C bus:
  // SDA = GPIO21
  // SCL = GPIO22
  Wire.begin(21, 22);

  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (true);
  }

  // Display startup splash screen
  showSplash();

  // Measure initial ADC offsets
  calibrateZero();
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {

  // Capture ADC samples
  captureSamples();

  // Clear entire OLED framebuffer
  display.clearDisplay();

  // Draw graph frame and divisions
  drawGrid();

  // Draw dotted axes
  drawAxesDots();

  // Draw V/I curve
  drawCurve();

  // Draw information text
  drawInfo();

  // Transfer framebuffer to OLED
  display.display();

  // Small delay to reduce flicker
  delay(50);
}

// =============================================================================
// SPLASH SCREEN
// =============================================================================

void showSplash() {

  // Clear framebuffer
  display.clearDisplay();

  // Large title
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);

  display.setCursor(0, 12);
  display.println("V/I");
  display.println("analyzer");

  // Version line
  display.setTextSize(1);

  display.setCursor(0, 52);
  display.println("v1.0");

  // Send to display
  display.display();

  // Keep splash visible
  delay(2000);
}

// =============================================================================
// ZERO CALIBRATION
// =============================================================================

void calibrateZero() {

  long sumV = 0;
  long sumI = 0;

  const int samples = 200;

  // Average several samples
  // to obtain stable ADC midpoint
  for (int n = 0; n < samples; n++) {

    sumV += analogRead(PIN_V);
    sumI += analogRead(PIN_I);

    delay(2);
  }

  // Store average values
  zeroV = sumV / samples;
  zeroI = sumI / samples;
}

// =============================================================================
// SAMPLE ACQUISITION
// =============================================================================

void captureSamples() {

  for (int n = 0; n < N; n++) {

    // Read voltage channel
    int adcV = analogRead(PIN_V);

    // Read current channel
    int adcI = analogRead(PIN_I);

    // Convert ADC values into screen coordinates
    xPoints[n] = mapCenteredX(adcV, zeroV);

    // Current channel intentionally inverted
    yPoints[n] = mapCenteredYInverted(adcI, zeroI);
  }
}

// =============================================================================
// ADC -> SCREEN MAPPING
// =============================================================================

int mapCenteredX(int adc, int zero) {

  // Remove calibrated offset
  int delta = adc - zero;

  // Convert centered ADC value into X coordinate
  int x = CX + map(
              delta,
              -2048,
               2047,
              -GRID_SIZE / 2,
               GRID_SIZE / 2
            );

  // Prevent out-of-range drawing
  return constrain(
           x,
           GRID_X,
           GRID_X + GRID_SIZE
         );
}

int mapCenteredYInverted(int adc, int zero) {

  // Remove calibrated offset
  int delta = adc - zero;

  // Convert centered ADC value into Y coordinate
  // Y axis intentionally inverted
  int y = CY + map(
              delta,
              -2048,
               2047,
              -GRID_SIZE / 2,
               GRID_SIZE / 2
            );

  // Prevent out-of-range drawing
  return constrain(
           y,
           GRID_Y,
           GRID_Y + GRID_SIZE
         );
}

// =============================================================================
// GRID DRAWING
// =============================================================================

void drawGrid() {

  // Outer square
  display.drawRect(
    GRID_X,
    GRID_Y,
    GRID_SIZE,
    GRID_SIZE,
    SSD1306_WHITE
  );

  int step = GRID_SIZE / DIVS;

  // Draw division marks
  for (int i = 1; i < DIVS; i++) {

    int x = GRID_X + i * step;
    int y = GRID_Y + i * step;

    // Top ticks
    display.drawLine(
      x,
      GRID_Y,
      x,
      GRID_Y + TICK,
      SSD1306_WHITE
    );

    // Bottom ticks
    display.drawLine(
      x,
      GRID_Y + GRID_SIZE,
      x,
      GRID_Y + GRID_SIZE - TICK,
      SSD1306_WHITE
    );

    // Left ticks
    display.drawLine(
      GRID_X,
      y,
      GRID_X + TICK,
      y,
      SSD1306_WHITE
    );

    // Right ticks
    display.drawLine(
      GRID_X + GRID_SIZE,
      y,
      GRID_X + GRID_SIZE - TICK,
      y,
      SSD1306_WHITE
    );
  }
}

// =============================================================================
// AXES DRAWING
// =============================================================================

void drawAxesDots() {

  // Horizontal dotted axis
  for (int x = GRID_X;
       x <= GRID_X + GRID_SIZE;
       x += 4) {

    display.drawPixel(
      x,
      CY,
      SSD1306_WHITE
    );
  }

  // Vertical dotted axis
  for (int y = GRID_Y;
       y <= GRID_Y + GRID_SIZE;
       y += 4) {

    display.drawPixel(
      CX,
      y,
      SSD1306_WHITE
    );
  }
}

// =============================================================================
// CURVE DRAWING
// =============================================================================

void drawCurve() {

  // Draw connected line segments
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

// =============================================================================
// INFORMATION TEXT
// =============================================================================

void drawInfo() {

  // Calculate volts per division
  float voltsPerDiv =
    (2.0 * V_FULL_SCALE) / DIVS;

  // Convert to mA/div using shunt resistor
  float mAperDiv =
    (voltsPerDiv / R_SHUNT) * 1000.0;

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // Title
  display.setCursor(TEXT_X, 0);
  display.println("V/I graph");

  // Empty line intentionally omitted

  // Voltage scale
  display.setCursor(TEXT_X, 16);
  display.print("V: ");
  display.print(voltsPerDiv, 1);
  display.println("v");

  // Current scale
  display.setCursor(TEXT_X, 26);
  display.print("I: ");
  display.print(mAperDiv, 1);
  display.println("mA");
}
