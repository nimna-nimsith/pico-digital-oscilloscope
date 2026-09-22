#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// =====================================================
// TFT PIN CONFIGURATION  (matches your updated wiring)
// =====================================================
// TFT      | Pico
// VCC      | 3.3V
// GND      | GND
// SCL/SCK  | GP18   (hardware SPI clock)
// SDA/MOSI | GP19   (hardware SPI data)
// CS       | GP17
// DC / A0  | GP7
// RES/RST  | GP8
// LED/BL   | 3.3V

#define TFT_CS   17
#define TFT_DC    7
#define TFT_RST   8

// =====================================================
// ADC INPUT
// =====================================================

#define ADC_PIN 26

// 4-pin tactile push-button wired between this pin and GND.
// Toggles RUN/STOP on each press. INPUT_PULLUP means the
// pin reads HIGH when released and LOW when pressed.
#define BUTTON_PIN 14

// =====================================================
// TFT OBJECT
// =====================================================

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// =====================================================
// DISPLAY LAYOUT  (landscape, 160 x 128)
// =====================================================

#define SCREEN_WIDTH  160
#define SCREEN_HEIGHT 128

// Title bar
#define TITLE_H 14

// Oscilloscope graph
#define GRAPH_X       2
#define GRAPH_Y       (TITLE_H + 2)
#define GRAPH_WIDTH   156
#define GRAPH_HEIGHT  72

// Bottom stats panel
#define PANEL_Y       (GRAPH_Y + GRAPH_HEIGHT + 2)
#define PANEL_H       (SCREEN_HEIGHT - PANEL_Y)

// =====================================================
// COLOR PALETTE
// =====================================================

uint16_t COLOR_BG;
uint16_t COLOR_TITLE_BG;
uint16_t COLOR_GRID;
uint16_t COLOR_GRID_CENTER;
uint16_t COLOR_TRACE;
uint16_t COLOR_BORDER;
uint16_t COLOR_VPP;
uint16_t COLOR_FREQ;
uint16_t COLOR_MAXMIN;
uint16_t COLOR_RMS;
uint16_t COLOR_AVG;
uint16_t COLOR_LABEL;

void setupColors()
{
  COLOR_BG          = ST77XX_BLACK;
  COLOR_TITLE_BG    = tft.color565(8, 18, 40);
  COLOR_GRID        = tft.color565(0, 55, 80);
  COLOR_GRID_CENTER = tft.color565(0, 110, 150);
  COLOR_TRACE       = tft.color565(60, 255, 140);
  COLOR_BORDER      = tft.color565(60, 90, 120);
  COLOR_VPP         = ST77XX_CYAN;
  COLOR_FREQ        = ST77XX_YELLOW;
  COLOR_MAXMIN      = ST77XX_WHITE;
  COLOR_RMS         = tft.color565(255, 100, 220);
  COLOR_AVG         = tft.color565(150, 180, 255);
  COLOR_LABEL       = tft.color565(120, 130, 140);
}

// =====================================================
// ADC SETTINGS
// =====================================================

#define ADC_MAX_VALUE 4095.0
#define ADC_REFERENCE 3.3

// =====================================================
// VOLTAGE DIVIDER
// =====================================================
//
//  Input
//    |
//  100k
//    |
//    +------ ADC
//    |
//   33k
//    |
//   GND

#define R_TOP     100000.0
#define R_BOTTOM   33000.0
#define DIVIDER_FACTOR ((R_TOP + R_BOTTOM) / R_BOTTOM)

// =====================================================
// SAMPLE SETTINGS
// =====================================================

#define SAMPLE_COUNT 156           // matches GRAPH_WIDTH
#define SAMPLE_INTERVAL_US 100     // ~10 kSamples/s

uint16_t samples[SAMPLE_COUNT];

// =====================================================
// MEASUREMENTS
// =====================================================

float voltageMax = 0.0;
float voltageMin = 0.0;
float voltagePP  = 0.0;
float voltageAvg = 0.0;
float voltageRMS = 0.0;

float frequency = 0.0;
float periodUs  = 0.0;

bool running = true;
bool lastBtnState = HIGH;
unsigned long lastBtnMillis = 0;

// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  analogReadResolution(12);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  setupColors();

  tft.fillScreen(COLOR_BG);
  drawSplashScreen();
  delay(1200);

  tft.fillScreen(COLOR_BG);
  drawStaticUI();     // title bar, graph border, grid, panel labels — drawn once
  drawRunBadge();
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop()
{
  handleRunStopButton();

  if (running)
  {
    captureSamples();
    calculateVoltage();
    calculateRMS();
    calculateFrequency();
    updateGraph();
    updateMeasurements();
    printMeasurements();
  }
}

// =====================================================
// RUN / STOP BUTTON
// =====================================================

void handleRunStopButton()
{
  bool state = digitalRead(BUTTON_PIN);

  // Debounced press: only fires once per press, not while held
  if (state == LOW && lastBtnState == HIGH &&
      (millis() - lastBtnMillis) > 200)
  {
    running = !running;
    lastBtnMillis = millis();

    Serial.println("BUTTON PRESSED");
    Serial.println(running ? "STATE: RUN" : "STATE: STOP");

    drawRunBadge();
  }

  if (state == HIGH && lastBtnState == LOW)
  {
    Serial.println("BUTTON RELEASED");
  }

  lastBtnState = state;
}

// =====================================================
// CAPTURE ADC SAMPLES
// =====================================================

void captureSamples()
{
  unsigned long nextSampleTime = micros();

  for (int i = 0; i < SAMPLE_COUNT; i++)
  {
    while ((long)(micros() - nextSampleTime) < 0)
    {
      // wait
    }

    samples[i] = analogRead(ADC_PIN);
    nextSampleTime += SAMPLE_INTERVAL_US;
  }
}

// =====================================================
// ADC VALUE -> ORIGINAL INPUT VOLTAGE
// =====================================================

float adcToInputVoltage(uint16_t adcValue)
{
  float adcVoltage = ((float)adcValue / ADC_MAX_VALUE) * ADC_REFERENCE;
  return adcVoltage * DIVIDER_FACTOR;
}

// =====================================================
// CALCULATE VOLTAGE
// =====================================================

void calculateVoltage()
{
  voltageMax = -1000.0;
  voltageMin = 1000.0;
  float total = 0.0;

  for (int i = 0; i < SAMPLE_COUNT; i++)
  {
    float v = adcToInputVoltage(samples[i]);

    if (v > voltageMax) voltageMax = v;
    if (v < voltageMin) voltageMin = v;

    total += v;
  }

  voltagePP  = voltageMax - voltageMin;
  voltageAvg = total / SAMPLE_COUNT;
}

// =====================================================
// CALCULATE RMS
// =====================================================

void calculateRMS()
{
  float sumSquares = 0.0;

  for (int i = 0; i < SAMPLE_COUNT; i++)
  {
    float v = adcToInputVoltage(samples[i]);
    sumSquares += v * v;
  }

  voltageRMS = sqrt(sumSquares / SAMPLE_COUNT);
}

// =====================================================
// FREQUENCY DETECTION
// =====================================================

void calculateFrequency()
{
  if (voltagePP < 0.05)
  {
    frequency = 0;
    periodUs = 0;
    return;
  }

  float threshold = (voltageMax + voltageMin) / 2.0;

  int risingEdges = 0;
  unsigned long firstEdge = 0;
  unsigned long lastEdge = 0;
  bool previousState = false;

  for (int i = 0; i < SAMPLE_COUNT; i++)
  {
    float v = adcToInputVoltage(samples[i]);
    bool currentState = v > threshold;

    if (currentState && !previousState)
    {
      unsigned long edgeTime = i * SAMPLE_INTERVAL_US;

      if (risingEdges == 0) firstEdge = edgeTime;
      lastEdge = edgeTime;
      risingEdges++;
    }

    previousState = currentState;
  }

  if (risingEdges >= 2)
  {
    periodUs = (float)(lastEdge - firstEdge) / (float)(risingEdges - 1);
    frequency = (periodUs > 0) ? (1000000.0 / periodUs) : 0;
  }
  else
  {
    frequency = 0;
    periodUs = 0;
  }
}

// =====================================================
// SPLASH SCREEN
// =====================================================

void drawSplashScreen()
{
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(18, 35);
  tft.print("PICO");

  tft.setTextSize(1);
  tft.setCursor(14, 58);
  tft.print("DIGITAL OSCILLOSCOPE");

  tft.setTextColor(COLOR_LABEL);
  tft.setCursor(30, 80);
  tft.print("initializing...");
}

// =====================================================
// STATIC UI  (drawn once — title bar, graph frame, grid,
// bottom panel dividers & labels)
// =====================================================

void drawStaticUI()
{
  // ---- Title bar ----
  tft.fillRect(0, 0, SCREEN_WIDTH, TITLE_H, COLOR_TITLE_BG);
  tft.drawFastHLine(0, TITLE_H, SCREEN_WIDTH, COLOR_BORDER);

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(3, 3);
  tft.print("PICO SCOPE");

  // ---- Graph border ----
  tft.drawRect(GRAPH_X - 1, GRAPH_Y - 1,
               GRAPH_WIDTH + 2, GRAPH_HEIGHT + 2, COLOR_BORDER);

  drawGrid();

  // ---- Bottom panel divider ----
  tft.drawFastHLine(0, PANEL_Y - 1, SCREEN_WIDTH, COLOR_BORDER);
  tft.drawFastVLine(80, PANEL_Y, PANEL_H, COLOR_BORDER);

  // Row divider inside panel
  int rowH = PANEL_H / 3;
  tft.drawFastHLine(0, PANEL_Y + rowH, SCREEN_WIDTH, tft.color565(25, 25, 30));
  tft.drawFastHLine(0, PANEL_Y + rowH * 2, SCREEN_WIDTH, tft.color565(25, 25, 30));

  // ---- Static labels ----
  tft.setTextColor(COLOR_LABEL);

  tft.setCursor(3, PANEL_Y + 3);
  tft.print("Vpp");

  tft.setCursor(83, PANEL_Y + 3);
  tft.print("FREQ");

  tft.setCursor(3, PANEL_Y + rowH + 3);
  tft.print("MAX");

  tft.setCursor(83, PANEL_Y + rowH + 3);
  tft.print("MIN");

  tft.setCursor(3, PANEL_Y + rowH * 2 + 3);
  tft.print("RMS");

  tft.setCursor(83, PANEL_Y + rowH * 2 + 3);
  tft.print("AVG");
}

// =====================================================
// RUN / STOP BADGE  (top-right of title bar)
// =====================================================

void drawRunBadge()
{
  int bw = 34, bh = 10;
  int bx = SCREEN_WIDTH - bw - 3;
  int by = 2;

  uint16_t badgeColor = running ? tft.color565(0, 90, 0) : tft.color565(110, 0, 0);
  uint16_t textColor  = running ? ST77XX_GREEN : ST77XX_RED;

  tft.fillRoundRect(bx, by, bw, bh, 3, badgeColor);
  tft.drawRoundRect(bx, by, bw, bh, 3, textColor);

  tft.setTextColor(textColor);
  tft.setCursor(bx + 3, by + 2);
  tft.print(running ? "RUN" : "STOP");
}

// =====================================================
// GRID  (drawn once as part of static UI)
// =====================================================

void drawGrid()
{
  for (int x = GRAPH_X + 26; x < GRAPH_X + GRAPH_WIDTH; x += 26)
  {
    tft.drawFastVLine(x, GRAPH_Y, GRAPH_HEIGHT, COLOR_GRID);
  }

  for (int y = GRAPH_Y + 18; y < GRAPH_Y + GRAPH_HEIGHT; y += 18)
  {
    tft.drawFastHLine(GRAPH_X, y, GRAPH_WIDTH, COLOR_GRID);
  }

  // Highlighted center line
  int centerY = GRAPH_Y + GRAPH_HEIGHT / 2;
  tft.drawFastHLine(GRAPH_X, centerY, GRAPH_WIDTH, COLOR_GRID_CENTER);
}

// =====================================================
// UPDATE GRAPH  (only redraws the graph interior each frame)
// =====================================================

void updateGraph()
{
  // Clear just the interior of the graph, keep border intact
  tft.fillRect(GRAPH_X, GRAPH_Y, GRAPH_WIDTH, GRAPH_HEIGHT, COLOR_BG);
  drawGrid();
  drawWaveform();
}

void drawWaveform()
{
  for (int x = 1; x < SAMPLE_COUNT; x++)
  {
    int y1 = map(samples[x - 1], 0, 4095,
                 GRAPH_Y + GRAPH_HEIGHT - 2, GRAPH_Y + 2);
    int y2 = map(samples[x], 0, 4095,
                 GRAPH_Y + GRAPH_HEIGHT - 2, GRAPH_Y + 2);

    y1 = constrain(y1, GRAPH_Y + 1, GRAPH_Y + GRAPH_HEIGHT - 1);
    y2 = constrain(y2, GRAPH_Y + 1, GRAPH_Y + GRAPH_HEIGHT - 1);

    tft.drawLine(GRAPH_X + x - 1, y1, GRAPH_X + x, y2, COLOR_TRACE);
  }
}

// =====================================================
// UPDATE MEASUREMENTS  (clears only the value area, not
// the whole panel, so labels never flicker)
// =====================================================

void printValue(int x, int y, int w, uint16_t color, const char* text)
{
  tft.fillRect(x, y, w, 9, COLOR_BG);   // clear old value only
  tft.setTextColor(color);
  tft.setCursor(x, y);
  tft.print(text);
}

void updateMeasurements()
{
  int rowH = PANEL_H / 3;
  char buf[16];

  // Vpp
  snprintf(buf, sizeof(buf), "%.2fV", voltagePP);
  printValue(30, PANEL_Y + 3, 48, COLOR_VPP, buf);

  // Frequency
  if (frequency < 1000)
    snprintf(buf, sizeof(buf), "%.0fHz", frequency);
  else
    snprintf(buf, sizeof(buf), "%.2fkHz", frequency / 1000.0);
  printValue(112, PANEL_Y + 3, 46, COLOR_FREQ, buf);

  // Max
  snprintf(buf, sizeof(buf), "%.2fV", voltageMax);
  printValue(30, PANEL_Y + rowH + 3, 48, COLOR_MAXMIN, buf);

  // Min
  snprintf(buf, sizeof(buf), "%.2fV", voltageMin);
  printValue(112, PANEL_Y + rowH + 3, 46, COLOR_MAXMIN, buf);

  // RMS
  snprintf(buf, sizeof(buf), "%.2fV", voltageRMS);
  printValue(30, PANEL_Y + rowH * 2 + 3, 48, COLOR_RMS, buf);

  // Avg
  snprintf(buf, sizeof(buf), "%.2fV", voltageAvg);
  printValue(112, PANEL_Y + rowH * 2 + 3, 46, COLOR_AVG, buf);
}

// =====================================================
// SERIAL MONITOR
// =====================================================

void printMeasurements()
{
  Serial.println("-----------------------------");
  Serial.print("Vmax: ");  Serial.print(voltageMax, 3);  Serial.println(" V");
  Serial.print("Vmin: ");  Serial.print(voltageMin, 3);  Serial.println(" V");
  Serial.print("Vpp: ");   Serial.print(voltagePP, 3);   Serial.println(" V");
  Serial.print("Average: "); Serial.print(voltageAvg, 3); Serial.println(" V");
  Serial.print("RMS: ");   Serial.print(voltageRMS, 3);  Serial.println(" V");
  Serial.print("Frequency: "); Serial.print(frequency, 2); Serial.println(" Hz");
  Serial.print("Period: "); Serial.print(periodUs, 2); Serial.println(" us");
}
