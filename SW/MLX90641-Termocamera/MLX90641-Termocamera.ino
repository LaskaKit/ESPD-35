/* 
 * This example code is used for LaskaKit ESPD-3.5 ESP32 3.5 TFT ILI9488 Touch v3.0 https://www.laskakit.cz/laskakit-espd-35-esp32-3-5-tft-ili9488-touch/
 * with LaskaKit MLX90641 Thermocamera 16x12
 * ESPD-3.5 board reads the camera values from the MLX90641 sensor and continuously draw it on display
 * 
 * Board:   LaskaKit ESPD-3.5 ESP32 3.5 TFT ILI9488 Touch   https://www.laskakit.cz/laskakit-espd-35-esp32-3-5-tft-ili9488-touch/
 * Sensor:  LaskaKit MLX90641 Thermocamera 16x12            https://www.laskakit.cz/laskakit-mlx90641-modul-termokamery-16--12px-55--x35/
 *
 * How to steps:
 * 1. Copy file Setup303_ILI9488_ESPD-3_5_v3.h from https://github.com/LaskaKit/ESPD-35/tree/main/SW to Arduino/libraries/TFT_eSPI/User_Setups/  
 * 2. in Arduino/libraries/TFT_eSPI/User_Setup_Select.h 
      a. comment: #include <User_Setup.h>
      b. add: #include <User_Setups/Setup303_ILI9488_ESPD-3_5_v3.h>  // Setup file for LaskaKit ESPD-3.5" 320x480, ILI9488 V3
 * 
 * Board constants:
      TFT_BL          - LED back-light use: analogWrite(TFT_BL, TFT_BL_PWM);
      TOUCH_INT       - Touch interrupt pin
    * I2C (µŠup and devices (only from v3)):
      I2C_SDA         - Data pin 
      I2C_SCL         - Clock pin
    * SPI (µŠup (only from v3) and SD card):
      SPI_MISO        - MISO pin
      SPI_MOSI        - MOSI pin
      SPI_SCK         - Clock pin
      SPI_USUP_CS     - µŠup Chip Select pin (only from v3)
      SPI_SD_CS       - SD Card Chip Select pin
    * I2S (only from v3):
      I2S_LRC         - Word select a.k.a. left-right clock pin
      I2S_DOUT        - Serial data pin
      I2S_BCLK        - Serial clock a.k.a. bit clock pin
    * Battery mesurement:
      BAT_PIN         - Battery voltage mesurement
      deviderRatio    - Voltage devider ratio on ADC pin 1MOhm + 1.3MOhm
 *
 * Email:podpora@laskakit.cz
 * Web:laskakit.cz
*/

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "MLX90641_API.h"
#include "MLX90641_I2C_Driver.h"

// ---------------- MLX / I2C ----------------
static const uint8_t  MLX_ADDR    = 0x33;
static const float    EMISSIVITY  = 0.95f;
static const uint8_t  MLX_REFRESH = 0x06;      // 0x03=8Hz, 0x04=16Hz, 0x05=32Hz, 0x06=64Hz
static const uint32_t I2C_INIT_HZ = 100000;    // 100k during init
static const uint32_t I2C_RUN_HZ  = 400000;    // 400k after init (MLX90641 supports up to 1 MHz)

// --- Battery measurement ---
static const int      BAT_SAMPLES     = 16;    // averaging
static const uint32_t BAT_INTERVAL_MS = 2000;  // battery changes slowly, no need to read it every frame

// ---------------- Screen layout ----------------
#define SCR_W   480
#define SCR_H   320
#define MARG    4
#define INFO_H  30          // bottom text row
#define BAR_W   14          // color bar width
#define GAP     4           // gap between image and bar

// Heatmap rectangle
#define HM_X    (MARG)
#define HM_Y    (MARG)
#define HM_W    (SCR_W - BAR_W - GAP - 2*MARG)
#define HM_H    (SCR_H - INFO_H - 2*MARG)

// Color bar rectangle
#define BAR_X   (HM_X + HM_W + GAP)
#define BAR_Y   (HM_Y)
#define BAR_H   (HM_H)

// Info row (bottom strip)
#define INFO_X  (0)
#define INFO_Y  (SCR_H - INFO_H)
#define INFO_W  (SCR_W)

// Flip orientation on the 16x12 buffer
#define FLIP_X  1   // 1 = horizontal mirror (left<->right)
#define FLIP_Y  1   // 1 = vertical mirror (top<->bottom)

// Color autoscale smoothing (reduces flicker)
#define CLIM_ALPHA 0.20f

// Crosshair
#define CROSS_SIZE 18

// Averaging of sensor frames (1 = no averaging). At 64 Hz, 4 frames -> 16 images/s
#define AVERAGE_N 4

// ---------------- Rendering notes ----------------
// ILI9488 over SPI needs 3 bytes (RGB666) per pixel. TFT_eSPI's pushImage() sends every
// pixel as three separate spi.transfer() calls and the per-pixel float colormap was slow
// too, so a single frame took several hundred ms. Now:
//   - the colormap is a 256-entry RGB LUT already in the display's byte format,
//   - bilinear upscaling uses integer math with precomputed column weights,
//   - each scanline is sent with one spi.writeBytes() inside a single address window,
//   - crosshair and temperature labels are composited into the scanline (no flicker),
//   - the sensor is read in its own task on core 0, so reading I2C and drawing overlap.

// ---------------- Globals ----------------
paramsMLX90641 gParams;
static uint16_t gEE[832];

TFT_eSPI tft;

// Latest averaged frame from the sensor task
static float             gShared[16*12];
static volatile bool     gSharedNew = false;
static SemaphoreHandle_t gLock;

// Rendering buffers
static uint8_t  lut[256][3];           // index 0..255 -> R,G,B bytes for the ILI9488
static uint8_t  lineBuf[HM_W * 3];     // one RGB666 scanline
static uint8_t  colX0[HM_W];           // source column for every display column
static uint16_t colWx[HM_W];           // weight of the right neighbour (0..256)
static uint8_t  rowY0[HM_H];
static uint16_t rowWy[HM_H];

// EMA display limits
static float vminEma = 20.0f, vmaxEma = 35.0f;

// Small text labels composited into the heatmap
#define LBL_W 72
#define LBL_H 16
struct Label {
  TFT_eSprite spr;
  int x = 0, y = 0, w = 0;             // relative to heatmap
  bool on = false;
  Label() : spr(&tft) {}
};
static Label lblHot, lblMax, lblMin;

// ---------------- Colormap ----------------
// Classic JET (blue->cyan->green->yellow->red), 8 bits per channel
static void buildLut() {
  for (int i = 0; i < 256; i++) {
    float t = i / 255.0f;
    float r = constrain(1.5f - fabsf(2.0f*t - 1.5f), 0.f, 1.f);
    float g = constrain(1.5f - fabsf(2.0f*t - 1.0f), 0.f, 1.f);
    float b = constrain(1.5f - fabsf(2.0f*t - 0.5f), 0.f, 1.f);
    lut[i][0] = (uint8_t)(r * 255.0f + 0.5f);
    lut[i][1] = (uint8_t)(g * 255.0f + 0.5f);
    lut[i][2] = (uint8_t)(b * 255.0f + 0.5f);
  }
}

static inline uint16_t lut565(int i) {
  return tft.color565(lut[i][0], lut[i][1], lut[i][2]);
}

// Precompute bilinear sampling positions (16x12 -> HM_W x HM_H)
static void buildScaleTables() {
  for (int dx = 0; dx < HM_W; dx++) {
    float fx = dx * 15.0f / (HM_W - 1);
    int x0 = min((int)fx, 14);
    colX0[dx] = x0;
    colWx[dx] = (uint16_t)((fx - x0) * 256.0f + 0.5f);
  }
  for (int dy = 0; dy < HM_H; dy++) {
    float fy = dy * 11.0f / (HM_H - 1);
    int y0 = min((int)fy, 10);
    rowY0[dy] = y0;
    rowWy[dy] = (uint16_t)((fy - y0) * 256.0f + 0.5f);
  }
}

// ---------------- Utils ----------------
static inline void flipInputHorizontal(float* img) { // 16x12
  for (int r=0; r<12; ++r)
    for (int c=0; c<8; ++c) {
      float t = img[r*16 + c];
      img[r*16 + c] = img[r*16 + (15 - c)];
      img[r*16 + (15 - c)] = t;
    }
}
static inline void flipInputVertical(float* img) { // 16x12
  for (int r=0; r<6; ++r)
    for (int c=0; c<16; ++c) {
      float t = img[r*16 + c];
      img[r*16 + c] = img[(11 - r)*16 + c];
      img[(11 - r)*16 + c] = t;
    }
}

// --- Battery measurement helper ---
static float readBatteryVoltage() {
  // analogReadMilliVolts uses the eFuse ADC calibration of the ESP32 core
  long sum_mV = 0;
  for (int i = 0; i < BAT_SAMPLES; ++i) {
    sum_mV += analogReadMilliVolts(BAT_PIN); // mV on the ADC pin (after divider)
    delayMicroseconds(200);
  }
  float v_adc = (sum_mV / (float)BAT_SAMPLES) / 1000.0f; // V
  return v_adc * deviderRatio;                           // battery voltage
}

// ---------------- Sensor task (core 0) ----------------
static void sensorTask(void*) {
  static uint16_t frame[242];
  static float    to[16*12];
  static float    acc[16*12];
  int n = 0;

  for (;;) {
    int st = MLX90641_GetFrameData(MLX_ADDR, frame);
    if (st < 0) {
      Serial.printf("GetFrameData error %d\n", st);
      vTaskDelay(pdMS_TO_TICKS(40));
      continue;
    }

    float Ta = MLX90641_GetTa(frame, &gParams);
    MLX90641_CalculateTo(frame, &gParams, EMISSIVITY, Ta, to);

#if FLIP_X
    flipInputHorizontal(to);
#endif
#if FLIP_Y
    flipInputVertical(to);
#endif

    for (int i = 0; i < 192; i++) acc[i] += to[i];
    if (++n >= max(1, AVERAGE_N)) {
      xSemaphoreTake(gLock, portMAX_DELAY);
      for (int i = 0; i < 192; i++) gShared[i] = acc[i] / n;
      gSharedNew = true;
      xSemaphoreGive(gLock);
      memset(acc, 0, sizeof(acc));
      n = 0;
    }
    vTaskDelay(1);   // let the idle task run (watchdog)
  }
}

// ---------------- Drawing ----------------

// Static part of the color bar – the gradient never changes, only the labels do
static void drawColorBarStatic() {
  for (int yy = 0; yy < BAR_H; ++yy) {
    int i = 255 - (yy * 255) / (BAR_H - 1);       // top = max
    tft.drawFastHLine(BAR_X, BAR_Y + yy, BAR_W, lut565(i));
  }
  tft.drawRect(BAR_X, BAR_Y, BAR_W, BAR_H, TFT_WHITE);
}

static void setLabel(Label& l, const char* text, int x, int y, uint16_t color) {
  l.spr.fillSprite(TFT_BLACK);
  l.spr.setTextColor(color, TFT_BLACK);
  l.spr.setTextDatum(TL_DATUM);
  l.spr.drawString(text, 2, 0, 2);
  l.w  = min(LBL_W, l.spr.textWidth(text, 2) + 4);
  l.x  = constrain(x, 0, HM_W - l.w);
  l.y  = constrain(y, 0, HM_H - LBL_H);
  l.on = true;
}

// Copy a label row into the scanline (sprite pixels are stored byte-swapped RGB565)
static inline void blendLabel(Label& l, int dy) {
  if (!l.on || dy < l.y || dy >= l.y + LBL_H) return;
  const uint16_t* src = (const uint16_t*)l.spr.getPointer() + (dy - l.y) * LBL_W;
  uint8_t* dst = lineBuf + l.x * 3;
  for (int i = 0; i < l.w; i++) {
    uint16_t c = (src[i] >> 8) | (src[i] << 8);
    *dst++ = (c >> 8) & 0xF8;
    *dst++ = (c >> 3) & 0xFC;
    *dst++ = (c << 3) & 0xF8;
  }
}

// Bilinear upscale 16x12 -> HM_W x HM_H, crosshair and labels included, sent line by line
static void drawHeatmap(const float* img) {
  float fmin = 1e9f, fmax = -1e9f;
  int hot = 0;
  for (int i = 0; i < 192; i++) {
    float v = img[i];
    if (v < fmin) fmin = v;
    if (v > fmax) { fmax = v; hot = i; }
  }
  if (fmax - fmin < 0.5f) { fmin -= 0.25f; fmax += 0.25f; }
  vminEma = (1.0f-CLIM_ALPHA)*vminEma + CLIM_ALPHA*fmin;
  vmaxEma = (1.0f-CLIM_ALPHA)*vmaxEma + CLIM_ALPHA*fmax;
  if (vmaxEma - vminEma < 0.5f) { float mid=0.5f*(vminEma+vmaxEma); vminEma=mid-0.25f; vmaxEma=mid+0.25f; }

  // temperatures -> colormap index in 8.8 fixed point
  int32_t q[192];
  const float scale = 255.0f * 256.0f / (vmaxEma - vminEma);
  for (int i = 0; i < 192; i++) {
    // clamp well outside 0..255 so the integer interpolation can never overflow
    float v = (img[i] - vminEma) * scale;
    q[i] = (int32_t)constrain(v, -65536.0f, 131072.0f);
  }

  // crosshair at the hottest pixel
  const int cx = (int)lroundf((hot % 16) * (HM_W - 1) / 15.0f);
  const int cy = (int)lroundf((hot / 16) * (HM_H - 1) / 11.0f);
  const int chx0 = max(0, cx - CROSS_SIZE), chx1 = min(HM_W - 1, cx + CROSS_SIZE);
  const int chy0 = max(0, cy - CROSS_SIZE), chy1 = min(HM_H - 1, cy + CROSS_SIZE);

  char buf[24];
  snprintf(buf, sizeof(buf), "%.2f C", fmax);
  setLabel(lblHot, buf, cx + 6, cy + 6, TFT_WHITE);
  snprintf(buf, sizeof(buf), "%.1f C", vmaxEma);
  setLabel(lblMax, buf, HM_W, 0, TFT_WHITE);          // right-aligned (clamped)
  snprintf(buf, sizeof(buf), "%.1f C", vminEma);
  setLabel(lblMin, buf, HM_W, HM_H, TFT_WHITE);

  SPIClass& spi = tft.getSPIinstance();
  tft.startWrite();
  tft.setAddrWindow(HM_X, HM_Y, HM_W, HM_H);

  int32_t row[16];
  for (int dy = 0; dy < HM_H; ++dy) {
    const int32_t* a = q + rowY0[dy] * 16;
    const int32_t* b = a + 16;
    const int32_t wy = rowWy[dy];
    for (int c = 0; c < 16; c++) row[c] = a[c] + (((b[c] - a[c]) * wy) >> 8);

    uint8_t* p = lineBuf;
    for (int dx = 0; dx < HM_W; ++dx) {
      const int x0 = colX0[dx];
      int32_t v = (row[x0] + (((row[x0 + 1] - row[x0]) * (int32_t)colWx[dx]) >> 8)) >> 8;
      if (v < 0) v = 0; else if (v > 255) v = 255;
      const uint8_t* c = lut[v];
      *p++ = c[0]; *p++ = c[1]; *p++ = c[2];
    }

    if (dy == cy) {
      memset(lineBuf + chx0 * 3, 0xFF, (chx1 - chx0 + 1) * 3);
    } else if (dy >= chy0 && dy <= chy1) {
      memset(lineBuf + cx * 3, 0xFF, 3);
    }
    blendLabel(lblMax, dy);
    blendLabel(lblMin, dy);
    blendLabel(lblHot, dy);

    spi.writeBytes(lineBuf, sizeof(lineBuf));
  }
  tft.endWrite();
}

// Bottom info row – text with padding overwrites the old one in place (no fillRect flicker)
static void drawInfoRow(const float* img, float fps) {
  static uint32_t lastBat = 0;
  static float    vbat    = 0;
  if (lastBat == 0 || millis() - lastBat >= BAT_INTERVAL_MS) {
    lastBat = millis();
    vbat = readBatteryVoltage();
  }

  float tMin = 1e9f, tMax = -1e9f, sum = 0.f;
  for (int i = 0; i < 192; i++) { float v = img[i]; if (v < tMin) tMin = v; if (v > tMax) tMax = v; sum += v; }
  float avg = sum / 192.f;

  tft.setTextFont(2);

  char l1[80];
  snprintf(l1, sizeof(l1), "MLX90641 16x12  min=%.2f  max=%.2f  avg=%.2f  %.1f fps", tMin, tMax, avg, fps);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextPadding(INFO_W - 120);
  tft.drawString(l1, INFO_X + 6, INFO_Y + 1);

  char l2[32];
  snprintf(l2, sizeof(l2), "VBAT=%.2f V", vbat);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextDatum(TR_DATUM);
  tft.setTextPadding(100);
  tft.drawString(l2, INFO_X + INFO_W - 6, INFO_Y + 1);
  tft.setTextPadding(0);
}

static void i2cScan() {
  Serial.println("I2C scan:");
  int found=0;
  for (uint8_t a=1;a<127;a++) { Wire.beginTransmission(a); if (Wire.endTransmission()==0){ Serial.printf("  0x%02X\n", a); found++; } }
  Serial.printf("Found %d device(s)\n", found);
}

// ---------------- Arduino lifecycle ----------------
void setup() {

  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && (millis()-t0 < 2000)) { delay(10); }

  tft.init();
  tft.setRotation(3);                 // ILI9488 landscape 480x320
  tft.fillScreen(TFT_BLACK);

  buildLut();
  buildScaleTables();
  for (Label* l : {&lblHot, &lblMax, &lblMin}) {
    l->spr.setColorDepth(16);
    l->spr.createSprite(LBL_W, LBL_H);
  }

  // --- Battery ADC setup ---
  analogReadResolution(12);                    // 12b (0..4095)
  analogSetPinAttenuation(BAT_PIN, ADC_11db);  // ~0..3.3V on the ADC pin

  Wire.begin(I2C_SDA, I2C_SCL, I2C_INIT_HZ);
  Wire.setTimeOut(500);

  i2cScan();

  int st=-1; const int MAX_RETRY=5;
  for (int i=0;i<MAX_RETRY;i++){ delay(60); st=MLX90641_DumpEE(MLX_ADDR,gEE); if(st==0)break; Serial.printf("DumpEE try %d failed: %d\n", i+1, st); }
  if (st!=0){ tft.setTextColor(TFT_RED, TFT_BLACK); tft.drawString("DumpEE failed!", 4, 4, 2); while(1) delay(10); }
  st=MLX90641_ExtractParameters(gEE, &gParams);
  if (st!=0){ tft.setTextColor(TFT_RED, TFT_BLACK); tft.drawString("ExtractParameters failed!", 4, 24, 2); while(1) delay(10); }
  MLX90641_SetRefreshRate(MLX_ADDR, MLX_REFRESH);

  Wire.setClock(I2C_RUN_HZ);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("Init OK", 4, 4, 2);
  drawColorBarStatic();

  // Sensor reading runs on core 0, drawing on core 1 (loop) – both work at the same time
  gLock = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(sensorTask, "mlx", 8192, nullptr, 2, nullptr, 0);
}

void loop() {
  static float    img[16*12];
  static uint32_t fpsT0 = millis();
  static int      fpsFrames = 0;
  static float    fps = 0;

  if (!gSharedNew) { vTaskDelay(1); return; }
  xSemaphoreTake(gLock, portMAX_DELAY);
  memcpy(img, gShared, sizeof(img));
  gSharedNew = false;
  xSemaphoreGive(gLock);

  drawHeatmap(img);

  fpsFrames++;
  uint32_t now = millis();
  if (now - fpsT0 >= 1000) {
    fps = fpsFrames * 1000.0f / (now - fpsT0);
    fpsFrames = 0;
    fpsT0 = now;
  }
  static uint32_t lastInfo = 0;
  if (now - lastInfo >= 250) {                      // text 4x per second is plenty
    lastInfo = now;
    drawInfoRow(img, fps);
  }
}
