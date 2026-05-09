#include <TFT_eSPI.h>
#include <SPI.h>
#include <I2S.h>
#include <math.h>

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite frame = TFT_eSprite(&tft);

#define SCREEN_W 320
#define SCREEN_H 240

#define I2S_DATA_PIN   9
#define I2S_BCLK_PIN   10
#define I2S_LRCLK_PIN  11

I2S i2s(INPUT);

// -------------------- Audio Config --------------------
const int SAMPLE_RATE = 16000;
const int BLOCK_SIZE = 512;

const int SHIFT_AMOUNT = 12;
const float INPUT_GAIN = 4.0;

// Scope visual tuning
const float SCOPE_GAIN = 0.012;      // lower = flatter, higher = taller
const float SILENCE_GATE_RMS = 450;  // raise if silence still looks crazy

// -------------------- Audio State --------------------
int16_t samples[BLOCK_SIZE];

float dcOffset = 0;

float rms = 0;
float peak = 0;
float smoothRMS = 0;
float smoothPeak = 0;

int16_t minSample = 0;
int16_t maxSample = 0;

bool clipping = false;

int32_t softLimit(int32_t s) {
  const int32_t limit = 28000;

  if (s > limit) s = limit + (s - limit) / 8;
  if (s < -limit) s = -limit + (s + limit) / 8;

  if (s > 32767) s = 32767;
  if (s < -32768) s = -32768;

  return s;
}

void readAudioBlock() {
  int count = 0;

  int32_t minVal = 32767;
  int32_t maxVal = -32768;
  int32_t peakVal = 0;

  double sumSq = 0;
  clipping = false;

  while (count < BLOCK_SIZE) {
    if (i2s.available()) {
      int32_t raw = i2s.read();

      int32_t s = raw >> SHIFT_AMOUNT;

      // DC removal
      dcOffset = dcOffset * 0.995 + s * 0.005;
      s = s - dcOffset;

      // Digital gain
      s = s * INPUT_GAIN;

      // Soft limit
      s = softLimit(s);

      if (s >= 32767 || s <= -32768) clipping = true;

      samples[count] = (int16_t)s;

      if (s < minVal) minVal = s;
      if (s > maxVal) maxVal = s;

      int32_t absVal = abs(s);
      if (absVal > peakVal) peakVal = absVal;

      sumSq += (double)s * (double)s;
      count++;
    }
  }

  minSample = minVal;
  maxSample = maxVal;
  peak = peakVal;
  rms = sqrt(sumSq / BLOCK_SIZE);

  smoothRMS = smoothRMS * 0.88 + rms * 0.12;
  smoothPeak = smoothPeak * 0.80 + peak * 0.20;
}

void drawOscilloscope() {
  const int scopeX = 8;
  const int scopeY = 154;
  const int scopeW = 304;
  const int scopeH = 78;
  const int midY = scopeY + scopeH / 2;

  frame.drawRect(scopeX, scopeY, scopeW, scopeH, TFT_DARKGREY);

  // Grid
  for (int x = scopeX + 38; x < scopeX + scopeW; x += 38) {
    frame.drawLine(x, scopeY + 1, x, scopeY + scopeH - 2, TFT_DARKGREY);
  }

  for (int y = scopeY + 13; y < scopeY + scopeH; y += 13) {
    frame.drawLine(scopeX + 1, y, scopeX + scopeW - 2, y, TFT_DARKGREY);
  }

  // frame.drawLine(scopeX + 1, midY, scopeX + scopeW - 2, midY, TFT_GREEN);

  // Silence gate: do not magnify room noise
  bool quiet = smoothRMS < SILENCE_GATE_RMS;

  int lastY = midY;

  for (int x = 0; x < scopeW - 2; x++) {
    int startIndex = map(x, 0, scopeW - 2, 0, BLOCK_SIZE - 1);
    int endIndex = map(x + 1, 0, scopeW - 2, 0, BLOCK_SIZE - 1);

    if (endIndex <= startIndex) endIndex = startIndex + 1;
    if (endIndex >= BLOCK_SIZE) endIndex = BLOCK_SIZE - 1;

    long sum = 0;
    int n = 0;

    for (int i = startIndex; i <= endIndex; i++) {
      sum += samples[i];
      n++;
    }

    float avg = sum / (float)n;

    if (quiet) {
      avg *= 0.08;   // flatten silence instead of showing crazy noise
    }

    int y = midY - avg * SCOPE_GAIN;

    if (y < scopeY + 2) y = scopeY + 2;
    if (y > scopeY + scopeH - 3) y = scopeY + scopeH - 3;

    if (x > 0) {
      frame.drawLine(scopeX + x, lastY, scopeX + x + 1, y, TFT_CYAN);
    }

    lastY = y;
  }

  frame.setTextColor(TFT_WHITE, TFT_BLACK);
  frame.setCursor(scopeX + 4, scopeY + 4);
  frame.print("OSC");
}

void drawScreen() {
  frame.fillSprite(TFT_BLACK);

  frame.setTextColor(TFT_WHITE, TFT_BLACK);
  frame.setTextSize(1);

  frame.setCursor(8, 6);
  frame.print("Pico I2S Mic Diagnostic");

  frame.setCursor(8, 22);
  frame.print("Rate: ");
  frame.print(SAMPLE_RATE);
  frame.print(" Hz");

  frame.setCursor(8, 36);
  frame.print("Shift: >>");
  frame.print(SHIFT_AMOUNT);

  frame.setCursor(100, 36);
  frame.print("Gain: ");
  frame.print(INPUT_GAIN, 1);

  frame.setCursor(8, 54);
  frame.print("Min: ");
  frame.print(minSample);

  frame.setCursor(8, 68);
  frame.print("Max: ");
  frame.print(maxSample);

  frame.setCursor(8, 82);
  frame.print("RMS: ");
  frame.print((int)rms);

  frame.setCursor(8, 96);
  frame.print("Peak: ");
  frame.print((int)peak);

  if (clipping) {
    frame.setTextColor(TFT_RED, TFT_BLACK);
    frame.setCursor(230, 6);
    frame.print("CLIPPING");
    frame.setTextColor(TFT_WHITE, TFT_BLACK);
  }

  int rmsBar = map((int)smoothRMS, 0, 8000, 0, 180);
  if (rmsBar > 180) rmsBar = 180;

  frame.drawRect(8, 116, 184, 12, TFT_DARKGREY);
  frame.fillRect(10, 118, rmsBar, 8, TFT_GREEN);
  frame.setCursor(200, 118);
  frame.print("RMS");

  int peakBar = map((int)smoothPeak, 0, 30000, 0, 180);
  if (peakBar > 180) peakBar = 180;

  frame.drawRect(8, 134, 184, 12, TFT_DARKGREY);
  frame.fillRect(10, 136, peakBar, 8, TFT_CYAN);
  frame.setCursor(200, 136);
  frame.print("PEAK");

  drawOscilloscope();

  frame.pushSprite(0, 0);
}

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  frame.setColorDepth(8);
  frame.createSprite(SCREEN_W, SCREEN_H);

  i2s.setDATA(I2S_DATA_PIN);
  i2s.setBCLK(I2S_BCLK_PIN);
  i2s.setBitsPerSample(32);

  if (!i2s.begin(SAMPLE_RATE)) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(20, 100);
    tft.println("I2S failed");
    while (1);
  }
}

void loop() {
  readAudioBlock();
  drawScreen();

  Serial.print("min=");
  Serial.print(minSample);
  Serial.print(" max=");
  Serial.print(maxSample);
  Serial.print(" rms=");
  Serial.print((int)rms);
  Serial.print(" peak=");
  Serial.print((int)peak);
  Serial.print(" clipping=");
  Serial.println(clipping ? "YES" : "NO");
}
