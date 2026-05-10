#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED SSD1306 128x64 I2C
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDR     0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pines ADC ESP32
const int PIN_V = 34;
const int PIN_I = 35;

// Cuadrícula
const int GRID_X = 0;
const int GRID_Y = 0;
const int GRID_SIZE = 63;

const int DIVS = 10;
const int TICK = 2;

const int CX = GRID_X + GRID_SIZE / 2;
const int CY = GRID_Y + GRID_SIZE / 2;

// Muestras
const int N = 96;

int xPoints[N];
int yPoints[N];

void setup() {

  Serial.begin(115200);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (true);
  }

  display.clearDisplay();
  display.display();
}

void loop() {

  captureSamples();

  display.clearDisplay();

  drawGrid();
  drawAxesDots();
  drawCurve();

  display.display();

  delay(50);
}

void captureSamples() {

  for (int n = 0; n < N; n++) {

    int adcV = analogRead(PIN_V);
    int adcI = analogRead(PIN_I);

    xPoints[n] = mapAdcToX(adcV);

    // Canal I invertido
    yPoints[n] = map(adcI, 0, 4095,
                     GRID_Y,
                     GRID_Y + GRID_SIZE);
  }
}

int mapAdcToX(int adc) {

  return map(adc, 0, 4095,
             GRID_X,
             GRID_X + GRID_SIZE);
}

void drawGrid() {

  display.drawRect(GRID_X,
                   GRID_Y,
                   GRID_SIZE,
                   GRID_SIZE,
                   SSD1306_WHITE);

  int step = GRID_SIZE / DIVS;

  for (int i = 1; i < DIVS; i++) {

    int x = GRID_X + i * step;
    int y = GRID_Y + i * step;

    // marcas arriba y abajo
    display.drawLine(x,
                     GRID_Y,
                     x,
                     GRID_Y + TICK,
                     SSD1306_WHITE);

    display.drawLine(x,
                     GRID_Y + GRID_SIZE,
                     x,
                     GRID_Y + GRID_SIZE - TICK,
                     SSD1306_WHITE);

    // marcas laterales
    display.drawLine(GRID_X,
                     y,
                     GRID_X + TICK,
                     y,
                     SSD1306_WHITE);

    display.drawLine(GRID_X + GRID_SIZE,
                     y,
                     GRID_X + GRID_SIZE - TICK,
                     y,
                     SSD1306_WHITE);
  }
}

void drawAxesDots() {

  // eje horizontal punteado
  for (int x = GRID_X; x <= GRID_X + GRID_SIZE; x += 4) {

    display.drawPixel(x,
                      CY,
                      SSD1306_WHITE);
  }

  // eje vertical punteado
  for (int y = GRID_Y; y <= GRID_Y + GRID_SIZE; y += 4) {

    display.drawPixel(CX,
                      y,
                      SSD1306_WHITE);
  }
}

void drawCurve() {

  for (int n = 1; n < N; n++) {

    display.drawLine(xPoints[n - 1],
                     yPoints[n - 1],
                     xPoints[n],
                     yPoints[n],
                     SSD1306_WHITE);
  }
}
