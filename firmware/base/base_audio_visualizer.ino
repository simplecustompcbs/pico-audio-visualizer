// COMBINED SKETCH: Mode A (Hyperspace Stars + Outward Wave Trails) <-> Mode B (XP Spectrum FFT)
// Runs Mode A for 10 seconds, switches to Mode B for 10 seconds, repeats forever.
//
// Target: Raspberry Pi Pico (Earle Philhower RP2040 core) + ILI9341 (TFT_eSPI) + ICS43434 I2S mic
//
// Notes:
// - Uses ONE full-screen sprite to save RAM.
// - Reconfigures I2S sample rate when switching modes (end/begin).
// - Spectrum auto-calibrates ONCE at boot (2s), then runs normally when mode switches.
//
// Libraries:
// - TFT_eSPI
// - I2S (Philhower core)
// - arduinoFFT

#include <TFT_eSPI.h>
#include <I2S.h>
#include <arduinoFFT.h>
#include <math.h>

// ----------------- Display -----------------
TFT_eSPI tft;
TFT_eSprite spr = TFT_eSprite(&tft);

// ----------------- I2S -----------------
I2S i2s(INPUT);

static const int PIN_I2S_BCLK  = 10;
static const int PIN_I2S_LRCLK = 11; // must be BCLK+1 for this core
static const int PIN_I2S_DATA  = 9;

// ----------------- Mode Switching -----------------
enum Mode { MODE_STARWAVE = 0, MODE_SPECTRUM = 1 };
static Mode currentMode = MODE_STARWAVE;
static uint32_t modeStartMs = 0;
static const uint32_t MODE_MS = 10000;

// ----------------- Shared Helpers -----------------
static inline float clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }

// HSV->RGB565 (uses tft.color565)
static uint16_t hsvTo565(float h, float s, float v) {
  while (h < 0) h += 360.0f;
  while (h >= 360.0f) h -= 360.0f;

  float c = v * s;
  float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
  float m = v - c;

  float rp=0,gp=0,bp=0;
  if (h < 60)       { rp=c; gp=x; bp=0; }
  else if (h < 120) { rp=x; gp=c; bp=0; }
  else if (h < 180) { rp=0; gp=c; bp=x; }
  else if (h < 240) { rp=0; gp=x; bp=c; }
  else if (h < 300) { rp=x; gp=0; bp=c; }
  else              { rp=c; gp=0; bp=x; }

  uint8_t r = (uint8_t)lroundf((rp + m) * 255.0f);
  uint8_t g = (uint8_t)lroundf((gp + m) * 255.0f);
  uint8_t b = (uint8_t)lroundf((bp + m) * 255.0f);
  return tft.color565(r, g, b);
}

// Safely restart I2S at a new frequency
static bool setI2SFreq(uint32_t hz) {
  i2s.end();

  bool ok = true;
  ok &= i2s.setBCLK(PIN_I2S_BCLK);
  ok &= i2s.setDATA(PIN_I2S_DATA);
  ok &= i2s.setBitsPerSample(32);
  ok &= i2s.setFrequency((int)hz);

  if (!ok) return false;
  return i2s.begin();
}

// ============================================================
// MODE A: Hyperspace Stars + Outward Wave Trails (Code 1)
// ============================================================

static const float I2S_FS_STAR = 20000.0f;

// Color cycle
static float hueA = 0.0f;
static const float HUE_SPEED_A = 1.2f;

// Waveform
static float dcA = 0.0f;
static float autoGainA = 1.0f;

static float* waveBufA = nullptr;
static int waveCapA = 0;

static void ensureWaveBufA(int w) {
  if (w <= waveCapA) return;
  if (waveBufA) free(waveBufA);
  waveBufA = (float*)malloc(sizeof(float) * w);
  waveCapA = w;
  for (int i = 0; i < w; i++) waveBufA[i] = 0.0f;
}

static void captureWaveA(int w) {
  ensureWaveBufA(w);

  float peak = 0.0001f;
  for (int i = 0; i < w; i++) {
    int32_t l=0, r=0;
    i2s.read32(&l, &r);

    int32_t s = l >> 8;
    float x = (float)s / 8388608.0f;

    // DC block
    dcA = 0.995f * dcA + 0.005f * x;
    x -= dcA;

    waveBufA[i] = x;

    float a = fabsf(x);
    if (a > peak) peak = a;
  }

  float target = 0.60f;
  float g = target / peak;
  autoGainA = 0.90f * autoGainA + 0.10f * g;
}

// Wave trail history
static const int WAVE_TRAIL_FRAMES_A = 6;
static float* waveHistA[WAVE_TRAIL_FRAMES_A] = {0};
static int histWA = 0;
static int histHeadA = 0;

static const float TRAIL_PUSH_PER_FRAME_A = 1.4f;
static const float TRAIL_FADE_PER_FRAME_A = 0.20f;
static const int WAVE_THICK_NEW_A = 3;

static void ensureWaveHistA(int w) {
  if (w == histWA) return;

  for (int i = 0; i < WAVE_TRAIL_FRAMES_A; i++) {
    if (waveHistA[i]) { free(waveHistA[i]); waveHistA[i] = nullptr; }
  }
  histWA = w;
  histHeadA = 0;

  for (int i = 0; i < WAVE_TRAIL_FRAMES_A; i++) {
    waveHistA[i] = (float*)malloc(sizeof(float) * w);
    for (int x = 0; x < w; x++) waveHistA[i][x] = 0.0f;
  }
}

static void pushWaveToHistoryA(int w) {
  ensureWaveHistA(w);
  float* dst = waveHistA[histHeadA];
  for (int x = 0; x < w; x++) dst[x] = waveBufA[x];
  histHeadA = (histHeadA + 1) % WAVE_TRAIL_FRAMES_A;
}

static inline void unitOutwardL1A(float dx, float dy, float &ux, float &uy) {
  float denom = fabsf(dx) + fabsf(dy) + 0.001f;
  ux = dx / denom;
  uy = dy / denom;
}

static void drawWaveTrailsFastA(int w, int h, float hueNow) {
  const int cy = h / 2;
  const int amp = h / 6;
  const float cx = w * 0.5f;

  spr.drawFastHLine(0, cy, w, tft.color565(30, 30, 30));

  for (int age = WAVE_TRAIL_FRAMES_A - 1; age >= 0; age--) {
    int idx = histHeadA - 1 - age;
    while (idx < 0) idx += WAVE_TRAIL_FRAMES_A;
    idx %= WAVE_TRAIL_FRAMES_A;

    float* wf = waveHistA[idx];

    float v = 1.0f - (float)age * TRAIL_FADE_PER_FRAME_A;
    v = clamp01(v);
    if (v <= 0.02f) continue;

    uint16_t col = hsvTo565(hueNow, 0.95f, v);
    float push = (float)age * TRAIL_PUSH_PER_FRAME_A;

    int xStep = (age == 0) ? 1 : (age <= 2 ? 2 : 3);
    int thick = (age == 0) ? WAVE_THICK_NEW_A : 1;
    int half = thick / 2;

    for (int t = -half; t <= half; t++) {
      int px = 0;
      int py = cy + t;
      bool havePrev = false;

      for (int x = 0; x < w; x += xStep) {
        float vv = wf[x] * autoGainA;
        int y = (cy - (int)(vv * amp)) + t;

        float dx = (float)x - cx;
        float dy = (float)y - (float)cy;

        float ux, uy;
        unitOutwardL1A(dx, dy, ux, uy);

        int x2 = x + (int)lroundf(ux * push);
        int y2 = y + (int)lroundf(uy * push);

        if (!havePrev) {
          px = x2; py = y2; havePrev = true;
        } else {
          spr.drawLine(px, py, x2, y2, col);
          px = x2; py = y2;
        }
      }
    }
  }
}

// Stars
static const int   NUM_STARS_A = 260;
static const float SPEED_A     = 2.8f;
static const float LEN_GROW_A  = 1.2f;
static const float LEN_MAX_A   = 42.0f;

struct StarA {
  float xh, yh;
  float vx, vy;
  float len, spd;
};
static StarA starsA[NUM_STARS_A];

static uint32_t rngStateA = 0xA53C9E21u;

static inline uint32_t xorshift32A() {
  uint32_t x = rngStateA;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return rngStateA = x;
}
static inline float frandA(float a, float b) {
  float u = (float)(xorshift32A() >> 8) * (1.0f / 16777216.0f);
  return a + (b - a) * u;
}

static void respawnStarA(int i, int w, int h) {
  float cx = w * 0.5f;
  float cy = h * 0.5f;

  float x = frandA(0, (float)w);
  float y = frandA(0, (float)h);

  float vx = x - cx;
  float vy = y - cy;
  float mag = sqrtf(vx*vx + vy*vy) + 0.0001f;
  vx /= mag; vy /= mag;

  starsA[i].xh = x;
  starsA[i].yh = y;
  starsA[i].vx = vx;
  starsA[i].vy = vy;
  starsA[i].len = frandA(0, 6);
  starsA[i].spd = frandA(0.8f, 1.4f);
}

static void drawStarsA(int w, int h, uint16_t col) {
  for (int i = 0; i < NUM_STARS_A; i++) {
    StarA &s = starsA[i];

    if (s.len < LEN_MAX_A) {
      s.len += LEN_GROW_A * s.spd;
      if (s.len > LEN_MAX_A) s.len = LEN_MAX_A;
    }

    float step = SPEED_A * s.spd;
    s.xh += s.vx * step;
    s.yh += s.vy * step;

    float xt = s.xh - s.vx * s.len;
    float yt = s.yh - s.vy * s.len;

    bool off =
      (s.xh < -10 || s.xh > w+10 || s.yh < -10 || s.yh > h+10) &&
      (xt   < -10 || xt   > w+10 || yt   < -10 || yt   > h+10);

    if (off) {
      respawnStarA(i, w, h);
      continue;
    }

    spr.drawLine((int)xt, (int)yt, (int)s.xh, (int)s.yh, col);
    spr.drawPixel((int)s.xh, (int)s.yh, col);
  }
}

static void modeA_enter() {
  int w = tft.width();
  int h = tft.height();

  rngStateA ^= micros();
  for (int i = 0; i < NUM_STARS_A; i++) respawnStarA(i, w, h);

  ensureWaveBufA(w);
  ensureWaveHistA(w);

  hueA = 0.0f;
  dcA = 0.0f;
  autoGainA = 1.0f;

  spr.fillSprite(TFT_BLACK);
  spr.pushSprite(0, 0);
}

static void modeA_frame() {
  int w = tft.width();
  int h = tft.height();

  hueA += HUE_SPEED_A;
  if (hueA >= 360.0f) hueA -= 360.0f;
  uint16_t col = hsvTo565(hueA, 0.95f, 1.0f);

  captureWaveA(w);
  pushWaveToHistoryA(w);

  spr.fillSprite(TFT_BLACK);
  drawStarsA(w, h, col);
  drawWaveTrailsFastA(w, h, hueA);

  spr.pushSprite(0, 0);
}

// ============================================================
// MODE B: XP Spectrum FFT (Code 2) — rendered into full sprite
// ============================================================

static const float SAMPLE_RATE_B = 48000.0f;
static const uint16_t FFT_N_B = 1024;
static const uint16_t NUM_BARS_B = 40;

enum ChannelB { USE_LEFT_B, USE_RIGHT_B };
static const ChannelB MIC_CHANNEL_B = USE_LEFT_B;

static const float MIN_HZ_B = 60.0f;
static const float MAX_HZ_B = 10000.0f;
static const uint16_t MIN_BIN_B = 3;

static const float BAR_SMOOTHING_B  = 0.60f;
static const float PEAK_DECAY_B     = 0.92f;

static const float NF_ATTACK_B  = 0.50f;
static const float NF_RELEASE_B = 0.0018f;

static const uint32_t CAL_MS_B = 2000;

static float gateMarginDbB = 12.0f;
static const float LOW_GATE_EXTRA_DB_B = 8.0f;
static const int   LOW_GATE_BARS_B = 8;
static const float HARD_GATE_ABOVE_B = 1.8f;

static float rangeDbB = 18.0f;
static const float RANGE_DECAY_B = 0.995f;
static const float RANGE_MIN_B   = 14.0f;
static const float RANGE_MAX_B   = 40.0f;

static float agcGainDbB = 0.0f;
static const float AGC_SPEED_B = 0.04f;
static const float AGC_DECAY_SILENCE_B = 0.85f;
static const float AGC_RETURN_TO_0_B = 0.002f;
static const float AGC_MAX_GAIN_B = 16.0f;
static const float AGC_MIN_GAIN_B = -10.0f;
static const float SILENCE_ABOVE_THRESHOLD_B = 3.0f;

static const float BAR_GAMMA_B = 1.20f;

// UI layout within full sprite
static const int TOP_B = 28;
static const int BOTTOM_PAD_B = 8;

// FFT buffers
static float vRealB[FFT_N_B];
static float vImagB[FFT_N_B];
ArduinoFFT<float> FFTB(vRealB, vImagB, FFT_N_B, SAMPLE_RATE_B);

static uint16_t binStartB[NUM_BARS_B];
static uint16_t binEndB[NUM_BARS_B];
static float barCenterHzB[NUM_BARS_B];

static float eqDbB[NUM_BARS_B];
static uint16_t barColorB[NUM_BARS_B];

static float barValB[NUM_BARS_B];
static float peakValB[NUM_BARS_B];
static float noiseFloorDbB[NUM_BARS_B];

static void computeRainbowColorsB() {
  const float h0 = 0.0f, h1 = 270.0f;
  for (int i = 0; i < (int)NUM_BARS_B; i++) {
    float t = (NUM_BARS_B == 1) ? 0.0f : (float)i / (float)(NUM_BARS_B - 1);
    float hue = h0 + (h1 - h0) * t;
    barColorB[i] = hsvTo565(hue, 0.95f, 1.0f);
  }
}

// NOTE: drawHeaderB remains defined but is no longer used (text removed in Mode B)
static void drawHeaderB(const char* line2) {
  // draw into sprite (top area)
  spr.fillRect(0, 0, tft.width(), TOP_B, TFT_BLACK);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.setCursor(0, 0, 2);
  spr.println("XP Spectrum (Auto Level)");
  spr.setCursor(0, 14, 2);
  spr.println(line2);
}

static void captureAndFFTB() {
  float mean = 0.0f;

  for (uint16_t i = 0; i < FFT_N_B; i++) {
    int32_t l = 0, r = 0;
    i2s.read32(&l, &r);

    int32_t s = (MIC_CHANNEL_B == USE_LEFT_B) ? l : r;
    int32_t v24 = s >> 8;
    float x = (float)v24 / 8388608.0f;

    vRealB[i] = x;
    vImagB[i] = 0.0f;
    mean += x;
  }

  mean /= (float)FFT_N_B;
  for (uint16_t i = 0; i < FFT_N_B; i++) vRealB[i] -= mean;

  FFTB.windowing(FFTWindow::Hann, FFTDirection::Forward);
  FFTB.compute(FFTDirection::Forward);
  FFTB.complexToMagnitude();

  for (uint16_t k = 0; k < FFT_N_B / 2; k++) vRealB[k] /= (float)FFT_N_B;
}

static void computeBarsDbB(float* dbOut) {
  for (int b = 0; b < (int)NUM_BARS_B; b++) {
    uint16_t s = binStartB[b];
    uint16_t e = binEndB[b];
    if (e <= s) e = s + 1;

    double acc = 0.0;
    uint16_t n = 0;

    for (uint16_t k = s; k < e; k++) {
      float m = vRealB[k];
      acc += (double)m * (double)m;
      n++;
    }

    float mag = (n ? sqrtf((float)(acc / (double)n)) : 0.0f);
    float db = 20.0f * log10f(mag + 1e-9f);
    db += eqDbB[b];

    dbOut[b] = db;
  }
}

static void computeBarBinsHzB() {
  const float nyquist = SAMPLE_RATE_B * 0.5f;
  float maxHz = MAX_HZ_B;
  if (maxHz > nyquist) maxHz = nyquist;

  const float curvePow = 0.90f;
  const uint16_t MIN_BINS_PER_BAR = 3;

  for (int i = 0; i < (int)NUM_BARS_B; i++) {
    float t0 = (float)i / (float)NUM_BARS_B;
    float t1 = (float)(i + 1) / (float)NUM_BARS_B;
    t0 = powf(t0, curvePow);
    t1 = powf(t1, curvePow);

    float f0 = MIN_HZ_B * powf(maxHz / MIN_HZ_B, t0);
    float f1 = MIN_HZ_B * powf(maxHz / MIN_HZ_B, t1);

    uint16_t b0 = (uint16_t)floorf((f0 * FFT_N_B) / SAMPLE_RATE_B);
    uint16_t b1 = (uint16_t)floorf((f1 * FFT_N_B) / SAMPLE_RATE_B);

    if (b0 < MIN_BIN_B) b0 = MIN_BIN_B;
    if (b1 <= b0) b1 = b0 + 1;
    if ((uint16_t)(b1 - b0) < MIN_BINS_PER_BAR) b1 = b0 + MIN_BINS_PER_BAR;

    if (b1 >= FFT_N_B / 2) b1 = FFT_N_B / 2 - 1;

    if (i > 0 && b0 < binEndB[i - 1]) b0 = binEndB[i - 1];
    if (b1 <= b0) b1 = b0 + 1;
    if (b1 >= FFT_N_B / 2) b1 = FFT_N_B / 2 - 1;

    binStartB[i] = b0;
    binEndB[i]   = b1;
    barCenterHzB[i] = 0.5f * (f0 + f1);
  }
}

static void computeVisualizerEQB() {
  for (int i = 0; i < (int)NUM_BARS_B; i++) {
    float f = barCenterHzB[i];

    float lowAttenDb = 0.0f;
    if (f < 250.0f) {
      float x = f / 250.0f;
      lowAttenDb = -14.0f * (1.0f - x);
    }

    float highBoostDb = 0.0f;
    if (f > 1000.0f) {
      float x = logf(f / 1000.0f) / logf(MAX_HZ_B / 1000.0f);
      if (x < 0) x = 0; if (x > 1) x = 1;
      highBoostDb = 14.0f * x;
    }

    eqDbB[i] = lowAttenDb + highBoostDb;
  }
}

static void autoCalibrateB() {
  // Text removed: just show blank screen during calibration
  spr.fillSprite(TFT_BLACK);
  spr.pushSprite(0, 0);

  bool first = true;
  float dbFrame[NUM_BARS_B];

  uint32_t start = millis();
  while (millis() - start < CAL_MS_B) {
    captureAndFFTB();
    computeBarsDbB(dbFrame);

    if (first) {
      for (int i = 0; i < (int)NUM_BARS_B; i++) noiseFloorDbB[i] = dbFrame[i];
      first = false;
    } else {
      for (int i = 0; i < (int)NUM_BARS_B; i++) {
        if (dbFrame[i] < noiseFloorDbB[i]) noiseFloorDbB[i] = dbFrame[i];
      }
    }
    delay(10);
  }

  gateMarginDbB = 12.0f;
  rangeDbB = 18.0f;
  agcGainDbB = 0.0f;

  for (int i = 0; i < (int)NUM_BARS_B; i++) {
    barValB[i]  = 0.0f;
    peakValB[i] = 0.0f;
  }

  // Clear screen once after calibration (no header text)
  spr.fillSprite(TFT_BLACK);
  spr.pushSprite(0, 0);
}

static void modeB_enter() {
  // No text; just clear
  spr.fillSprite(TFT_BLACK);
  spr.pushSprite(0, 0);
}

static void modeB_frame() {
  captureAndFFTB();

  float dbRaw[NUM_BARS_B];
  computeBarsDbB(dbRaw);

  // Update noise floor
  for (int i = 0; i < (int)NUM_BARS_B; i++) {
    float x = dbRaw[i];
    float nf = noiseFloorDbB[i];
    if (x < nf) nf = (1.0f - NF_ATTACK_B) * nf + NF_ATTACK_B * x;
    else        nf = (1.0f - NF_RELEASE_B) * nf + NF_RELEASE_B * x;
    noiseFloorDbB[i] = nf;
  }

  // Mid/high above-floor for silence detect
  float aboveSum = 0.0f;
  int aboveCount = 0;
  for (int b = 16; b <= 32; b++) {
    float above = dbRaw[b] - noiseFloorDbB[b] - gateMarginDbB;
    if (above < 0) above = 0;
    aboveSum += above;
    aboveCount++;
  }
  float aboveAvg = (aboveCount ? (aboveSum / (float)aboveCount) : 0.0f);

  // AGC
  if (aboveAvg < SILENCE_ABOVE_THRESHOLD_B) {
    agcGainDbB *= AGC_DECAY_SILENCE_B;
  } else {
    float targetAbove = 14.0f;
    float err = targetAbove - aboveAvg;
    agcGainDbB += AGC_SPEED_B * err;
    agcGainDbB -= AGC_RETURN_TO_0_B * agcGainDbB;
  }
  if (agcGainDbB < AGC_MIN_GAIN_B) agcGainDbB = AGC_MIN_GAIN_B;
  if (agcGainDbB > AGC_MAX_GAIN_B) agcGainDbB = AGC_MAX_GAIN_B;

  // Bars + peaks
  float framePeak = 0.0f;
  for (int i = 0; i < (int)NUM_BARS_B; i++) {
    float above = dbRaw[i] - noiseFloorDbB[i] - gateMarginDbB;
    if (i < LOW_GATE_BARS_B) above -= LOW_GATE_EXTRA_DB_B;

    if (aboveAvg >= SILENCE_ABOVE_THRESHOLD_B) above += agcGainDbB;

    if (above < HARD_GATE_ABOVE_B) above = 0.0f;
    if (above > framePeak) framePeak = above;

    barValB[i] = BAR_SMOOTHING_B * barValB[i] + (1.0f - BAR_SMOOTHING_B) * above;

    if (barValB[i] > peakValB[i]) peakValB[i] = barValB[i];
    else peakValB[i] = PEAK_DECAY_B * peakValB[i] + (1.0f - PEAK_DECAY_B) * barValB[i];
  }

  // Range adaptation
  rangeDbB *= RANGE_DECAY_B;
  if (aboveAvg >= SILENCE_ABOVE_THRESHOLD_B) {
    if (framePeak > rangeDbB) rangeDbB = framePeak;
  }
  if (rangeDbB < RANGE_MIN_B) rangeDbB = RANGE_MIN_B;
  if (rangeDbB > RANGE_MAX_B) rangeDbB = RANGE_MAX_B;

  // Render into full sprite
  const int w = tft.width();
  const int h = tft.height();
  const int plotY = TOP_B;
  const int plotH = h - TOP_B - BOTTOM_PAD_B;

  // Clear entire sprite (no header text)
  spr.fillSprite(TFT_BLACK);

  const int BAR_GAP = 0;
  const int barW = (w - (NUM_BARS_B - 1) * BAR_GAP) / NUM_BARS_B;

  for (int i = 0; i < (int)NUM_BARS_B; i++) {
    float v  = barValB[i]  / rangeDbB;
    float pv = peakValB[i] / rangeDbB;

    v  = clamp01(v);
    pv = clamp01(pv);

    if (BAR_GAMMA_B > 0.01f && BAR_GAMMA_B != 1.0f) {
      v  = powf(v,  1.0f / BAR_GAMMA_B);
      pv = powf(pv, 1.0f / BAR_GAMMA_B);
    }

    int bh    = (int)(v  * plotH);
    int peakH = (int)(pv * plotH);

    int x = i * (barW + BAR_GAP);
    int yBottom = plotY + plotH;
    int yTop = yBottom - bh;
    int peakY = (plotY + (plotH - peakH));

    if (bh > 0) spr.fillRect(x, yTop, barW, bh, barColorB[i]);
    spr.fillRect(x, peakY, barW, 2, TFT_WHITE);
  }

  spr.pushSprite(0, 0);
  delay(10);
}

// ============================================================
// Boot + Mode switch glue
// ============================================================

static void switchMode(Mode next) {
  if (next == currentMode) return;

  // Change I2S rate for the next mode
  bool ok = false;
  if (next == MODE_STARWAVE) ok = setI2SFreq((uint32_t)I2S_FS_STAR);
  else                      ok = setI2SFreq((uint32_t)SAMPLE_RATE_B);

  // If I2S restart fails, show error and halt (better than silent black screen)
  if (!ok) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(0, 0, 2);
    tft.println("I2S restart FAILED");
    tft.setCursor(0, 14, 2);
    tft.println(next == MODE_STARWAVE ? "to STAR mode" : "to FFT mode");
    while (1) delay(1000);
  }

  currentMode = next;
  modeStartMs = millis();

  if (currentMode == MODE_STARWAVE) modeA_enter();
  else                             modeB_enter();
}

void setup() {
  Serial.begin(115200);

  // Display init
  tft.init();
  tft.setRotation(1);

  spr.setColorDepth(16);
  spr.createSprite(tft.width(), tft.height());
  spr.fillSprite(TFT_BLACK);
  spr.pushSprite(0, 0);

  // Sanity check for this I2S core setup
  if (PIN_I2S_LRCLK != PIN_I2S_BCLK + 1) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(0, 0, 2);
    tft.println("ERR: LRCLK != BCLK+1");
    while (1) delay(1000);
  }

  // Precompute spectrum tables
  computeBarBinsHzB();
  computeVisualizerEQB();
  computeRainbowColorsB();

  // Calibrate spectrum once at boot
  if (!setI2SFreq((uint32_t)SAMPLE_RATE_B)) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(0, 0, 2);
    tft.println("I2S init FAILED");
    while (1) delay(1000);
  }
  autoCalibrateB();

  // Start in Mode A (star/wave) as requested
  if (!setI2SFreq((uint32_t)I2S_FS_STAR)) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(0, 0, 2);
    tft.println("I2S init FAILED");
    while (1) delay(1000);
  }

  currentMode = MODE_STARWAVE;
  modeStartMs = millis();
  modeA_enter();
}

void loop() {
  // Switch every 10 seconds
  uint32_t now = millis();
  if (now - modeStartMs >= MODE_MS) {
    switchMode(currentMode == MODE_STARWAVE ? MODE_SPECTRUM : MODE_STARWAVE);
  }

  // Run active mode frame
  if (currentMode == MODE_STARWAVE) modeA_frame();
  else                              modeB_frame();
}
