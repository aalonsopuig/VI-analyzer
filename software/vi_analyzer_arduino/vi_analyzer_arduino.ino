/*
===============================================================================
Name:         vi_analyzer_arduino
Version:      1.0.0
Author:       Alejandro Alonso Puig (https://github.com/aalonsopuig) + GPT
Date:         2026-05
License:      Apache 2.0
-------------------------------------------------------------------------------
Description:

V/I curve analyzer for Arduino Mega using a 3.6" TFT LCD shield based on the
ILI9327 controller (400x240 pixels).

The system measures and displays the voltage/current characteristic curve of
electronic components using an X/Y graphical representation similar to the
XY mode of an oscilloscope.

The measurement principle is based on placing a fixed resistor in series with
the device under test (DUT):

    Function Generator --> Series Resistor --> DUT --> Return

Two analog channels are measured:

- A6:
  Voltage across the DUT (X axis)

- A7:
  Voltage across the series resistor (used to infer current, Y axis)

The analog front-end circuitry shifts bipolar input signals into the safe
0..5V range required by the Arduino Mega ADC.

The TFT display shows:
- square V/I graph area
- dotted center axes
- division marks
- voltage scale per division
- current scale per division
- series resistor value

Main characteristics:
- Arduino Mega 2560
- TFT shield ILI9327
- Resolution: 400x240
- Landscape orientation
- 10-bit ADC acquisition
- Automatic zero calibration at startup
- Continuous V/I curve refresh

Current display scaling:
- Horizontal axis: -5V .. +5V
- Vertical axis: current inferred from:
      I = V / 330 ohm

Typical observable curves:
- open circuit
- short circuit
- resistors
- capacitors
- inductors
- rectifier diodes
- LEDs
- zener diodes
- TRIACs

This project is intended as an educational and experimental proof of concept
focused on instrumentation, analog signal visualization and basic electronic
component characterization.

===============================================================================
*/

#include <MCUFRIEND_kbv.h>      // Driver library for many parallel TFT shields
#include <Adafruit_GFX.h>       // Generic graphics library used by MCUFRIEND_kbv

// -----------------------------------------------------------------------------
// TFT object
// -----------------------------------------------------------------------------

MCUFRIEND_kbv tft;              // Main TFT display object

// -----------------------------------------------------------------------------
// RGB565 color definitions
// -----------------------------------------------------------------------------

#define BLACK  0x0000           // Black background
#define WHITE  0xFFFF           // White text and axes
#define GREEN  0x07E0           // Green grid
#define RED    0xF800           // Red V/I curve

// -----------------------------------------------------------------------------
// Analog input pins
// -----------------------------------------------------------------------------

const int PIN_V = A6;           // Voltage channel: voltage across DUT
const int PIN_I = A7;           // Current channel: voltage across series resistor

// -----------------------------------------------------------------------------
// Measurement configuration
// -----------------------------------------------------------------------------

const float V_FULL_SCALE = 5.0; // Input range after conditioning: -5V..+5V

const float R_SHUNT = 330.0;    // Series resistor in ohms

// -----------------------------------------------------------------------------
// Graph layout
// -----------------------------------------------------------------------------

const int GRID_X = 10;          // Left coordinate of graph area
const int GRID_Y = 10;          // Top coordinate of graph area
const int GRID_SIZE = 220;      // Square graph size in pixels

const int DIVS = 10;            // Number of divisions per axis
const int TICK = 5;             // Length of division marks in pixels

const int CX = GRID_X + GRID_SIZE / 2;  // Graph center X coordinate
const int CY = GRID_Y + GRID_SIZE / 2;  // Graph center Y coordinate

// -----------------------------------------------------------------------------
// Text layout
// -----------------------------------------------------------------------------

const int TEXT_X = 250;         // Left coordinate of right-side text area
const int TEXT_Y = 20;          // Top coordinate of right-side text area

// -----------------------------------------------------------------------------
// Sampling configuration
// -----------------------------------------------------------------------------

const int N = 140;              // Number of points per V/I curve

int xPoints[N];                 // X screen coordinates of sampled curve
int yPoints[N];                 // Y screen coordinates of sampled curve

// -----------------------------------------------------------------------------
// ADC zero calibration
// -----------------------------------------------------------------------------

int zeroV = 512;                // Calibrated zero for voltage channel
int zeroI = 512;                // Calibrated zero for current channel

// =============================================================================
// SETUP
// =============================================================================

void setup() {

  Serial.begin(115200);         // Serial port used only for debug information

  uint16_t ID = tft.readID();   // Try to read TFT controller ID automatically

  Serial.print("TFT ID = 0x"); // Print detected TFT ID for troubleshooting
  Serial.println(ID, HEX);

  // Some TFT shields do not return a valid ID.
  // If this happens, force the known controller ID.
  if (ID == 0xD3D3 || ID == 0xFFFF || ID == 0x0000) {
    ID = 0x9327;                // ILI9327-compatible display
  }

  tft.begin(ID);                // Initialize TFT controller
  tft.setRotation(1);           // Landscape orientation

  showSplash();                 // Show startup screen

  calibrateZero();              // Measure ADC zero offsets

  tft.fillScreen(BLACK);        // Clear display after startup

  drawGrid();                   // Draw static graph frame
  drawAxesDots();               // Draw dotted central axes
  drawInfo();                   // Draw scale and resistor information

  captureSamples();             // Capture first curve
  drawCurve(RED);               // Draw first curve
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {

  delay(500);                   // Keep graph visible for readability

  drawCurve(BLACK);             // Erase previous curve

  captureSamples();             // Acquire new V/I sample set

  drawGrid();                   // Restore grid pixels erased by black curve
  drawAxesDots();               // Restore axis pixels erased by black curve

  drawCurve(RED);               // Draw refreshed V/I curve
}

// =============================================================================
// SPLASH SCREEN
// =============================================================================

void showSplash() {

  tft.fillScreen(BLACK);        // Clear screen

  tft.setTextColor(WHITE);      // White startup text
  tft.setTextSize(3);           // Large title font

  tft.setCursor(65, 85);        // Title position
  tft.print("V/I analyzer");    // Program title

  tft.setTextSize(2);           // Smaller version font
  tft.setCursor(165, 135);      // Version position
  tft.print("v1.0");            // Version label

  delay(2000);                  // Keep splash visible for 2 seconds
}

// =============================================================================
// ZERO CALIBRATION
// =============================================================================

void calibrateZero() {

  long sumV = 0;                // Accumulator for voltage channel
  long sumI = 0;                // Accumulator for current channel

  const int samples = 200;      // Number of samples used for averaging

  // During this step, the analog input should be in its zero condition.
  // The average value becomes the reference center of the graph.
  for (int n = 0; n < samples; n++) {

    sumV += analogRead(PIN_V);  // Read voltage channel
    sumI += analogRead(PIN_I);  // Read current channel

    delay(2);                   // Small delay to reduce noise influence
  }

  zeroV = sumV / samples;       // Store calibrated voltage zero
  zeroI = sumI / samples;       // Store calibrated current zero
}

// =============================================================================
// SAMPLE ACQUISITION
// =============================================================================

void captureSamples() {

  for (int n = 0; n < N; n++) {

    int adcV = analogRead(PIN_V);   // Read DUT voltage channel
    int adcI = analogRead(PIN_I);   // Read shunt voltage channel

    xPoints[n] = mapCenteredX(adcV, zeroV);          // Convert V to X coordinate
    yPoints[n] = mapCenteredYInverted(adcI, zeroI);  // Convert I to Y coordinate
  }
}

// =============================================================================
// ADC TO SCREEN MAPPING
// =============================================================================

int mapCenteredX(int adc, int zero) {

  int delta = adc - zero;       // Remove calibrated offset

  // The ADC is 10-bit: 0..1023.
  // After calibration, values around zero produce delta close to 0.
  // Full range is mapped to graph width.
  int x = CX + map(
    delta,
    -512,
     511,
    -GRID_SIZE / 2,
     GRID_SIZE / 2
  );

  // Ensure drawing remains inside the graph area.
  return constrain(x, GRID_X, GRID_X + GRID_SIZE);
}

int mapCenteredYInverted(int adc, int zero) {

  int delta = adc - zero;       // Remove calibrated offset

  // Screen Y coordinates grow downward.
  // This mapping intentionally inverts the current channel according to the
  // selected visual convention for the V/I plot.
  int y = CY + map(
    delta,
    -512,
     511,
    -GRID_SIZE / 2,
     GRID_SIZE / 2
  );

  // Ensure drawing remains inside the graph area.
  return constrain(y, GRID_Y, GRID_Y + GRID_SIZE);
}

// =============================================================================
// GRID DRAWING
// =============================================================================

void drawGrid() {

  // Draw outer square graph boundary.
  tft.drawRect(GRID_X, GRID_Y, GRID_SIZE, GRID_SIZE, GREEN);

  int step = GRID_SIZE / DIVS;  // Pixel spacing between division marks

  // Draw small tick marks on each side of the square.
  for (int i = 1; i < DIVS; i++) {

    int x = GRID_X + i * step;  // X position for top/bottom ticks
    int y = GRID_Y + i * step;  // Y position for left/right ticks

    tft.drawLine(x, GRID_Y, x, GRID_Y + TICK, GREEN);  // Top tick

    tft.drawLine(
      x,
      GRID_Y + GRID_SIZE,
      x,
      GRID_Y + GRID_SIZE - TICK,
      GREEN
    );                         // Bottom tick

    tft.drawLine(GRID_X, y, GRID_X + TICK, y, GREEN);  // Left tick

    tft.drawLine(
      GRID_X + GRID_SIZE,
      y,
      GRID_X + GRID_SIZE - TICK,
      y,
      GREEN
    );                         // Right tick
  }
}

// =============================================================================
// DOTTED AXES
// =============================================================================

void drawAxesDots() {

  // Horizontal dotted axis through graph center.
  for (int x = GRID_X; x <= GRID_X + GRID_SIZE; x += 7) {
    tft.drawPixel(x, CY, WHITE);
  }

  // Vertical dotted axis through graph center.
  for (int y = GRID_Y; y <= GRID_Y + GRID_SIZE; y += 7) {
    tft.drawPixel(CX, y, WHITE);
  }
}

// =============================================================================
// CURVE DRAWING
// =============================================================================

void drawCurve(uint16_t color) {

  // Draw the V/I curve as connected line segments.
  // Drawing in RED shows the current curve.
  // Drawing in BLACK erases the previous curve.
  for (int n = 1; n < N; n++) {

    tft.drawLine(
      xPoints[n - 1],
      yPoints[n - 1],
      xPoints[n],
      yPoints[n],
      color
    );
  }
}

// =============================================================================
// INFORMATION AREA
// =============================================================================

void drawInfo() {

  // The graph covers -5V..+5V, i.e. 10V total.
  // With 10 divisions, each division represents 1V.
  float voltsPerDiv = (2.0 * V_FULL_SCALE) / DIVS;

  // Current scale is calculated from the series resistor.
  // Example: 1V/div over 330 ohm gives 3.03mA/div.
  float mAperDiv = (voltsPerDiv / R_SHUNT) * 1000.0;

  tft.setTextColor(WHITE);      // White text
  tft.setTextSize(2);           // Readable size on 400x240 TFT

  tft.setCursor(TEXT_X, TEXT_Y);
  tft.print("V/I GRAPH");       // Main label

  tft.setCursor(TEXT_X, TEXT_Y + 50);
  tft.print(voltsPerDiv, 1);    // Voltage scale value
  tft.print(" v/div");          // Voltage scale unit

  tft.setCursor(TEXT_X, TEXT_Y + 85);
  tft.print(mAperDiv, 1);       // Current scale value
  tft.print(" mA/div");         // Current scale unit

  tft.setCursor(TEXT_X, TEXT_Y + 120);
  tft.print("R: ");             // Series resistor label
  tft.print((int)R_SHUNT);      // Series resistor value
}
