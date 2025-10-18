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
#include <TFT_eSPI.h>
#include "MLX90641_API.h"
#include "MLX90641_I2C_Driver.h"

// ---------------- MLX / I2C ----------------
static const uint8_t  MLX_ADDR    = 0x33;
static const float    EMISSIVITY  = 0.95f;
static const uint8_t  MLX_REFRESH = 0x06;      // 0x03=8Hz, 0x04=16Hz, 0x05=32Hz, 0x06=64Hz
static const uint32_t I2C_INIT_HZ = 100000;    // 100k during init
static const uint32_t I2C_RUN_HZ  = 400000;    // 400k after init

// --- Battery measurement ---
// Počty vzorků a attenuace pro stabilní a přesnější odečet
static const int   BAT_SAMPLES  = 16;   // průměrování

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
#define CROSS_COLOR TFT_WHITE

// Optional averaging (0 disables)
#define AVERAGE_N 4

// If your ILI9488 shows wrong hues, toggle this:
#define USE_BYTE_SWAP 1  // 1 works for most ILI9488 + TFT_eSPI setups

// ---------------- JET colormap ----------------
// Pack floats 0..1 to RGB565
static inline uint16_t pack565f(float r, float g, float b) {
  if (r<0) r=0; if (r>1) r=1;
  if (g<0) g=0; if (g>1) g=1;
  if (b<0) b=0; if (b>1) b=1;
  uint16_t R = (uint16_t)(r*31.0f + 0.5f);
  uint16_t G = (uint16_t)(g*63.0f + 0.5f);
  uint16_t B = (uint16_t)(b*31.0f + 0.5f);
  return (R<<11) | (G<<5) | B;
}

// Classic JET (blue->cyan->green->yellow->red)
static inline uint16_t jet565(float t) {
  if (t<0) t=0; if (t>1) t=1;
  float r = constrain(1.5f - fabsf(2.0f*t - 1.5f), 0.f, 1.f);
  float g = constrain(1.5f - fabsf(2.0f*t - 1.0f), 0.f, 1.f);
  float b = constrain(1.5f - fabsf(2.0f*t - 0.5f), 0.f, 1.f);
  return pack565f(r, g, b);
}

// Unified colormap call
static inline uint16_t color565_from_value(float v, float vmin, float vmax) {
  float t = (v - vmin) / (vmax - vmin);
  return jet565(t);
}

// ---------------- Globals ----------------
paramsMLX90641 gParams;
uint16_t gEE[832];
uint16_t gFrame[242];
float    gTo[16*12];

#if AVERAGE_N > 0
static float gAcc[16*12];
static int   gAccCount = 0;
#endif

TFT_eSPI tft;
static uint16_t lineBuf[HM_W];         // one RGB565 scanline

// Byte-swap helper for raw TFT primitives (color bar & text backgrounds)
#define SWAP565(c) (uint16_t)(((c) << 8) | ((c) >> 8))
static inline uint16_t maybeSwap(uint16_t c) { return USE_BYTE_SWAP ? SWAP565(c) : c; }

// EMA display limits
static float vminEma = 20.0f, vmaxEma = 35.0f;

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
  // Nastavíme swap pro pushImage, ale ADC čte nezávisle – jen připomínka, že je to separátní subsystém.
  // Použijeme analogReadMilliVolts (ESP32 Arduino core kalibruje podle eFuse).
  long sum_mV = 0;
  for (int i = 0; i < BAT_SAMPLES; ++i) {
    sum_mV += analogReadMilliVolts(BAT_PIN); // mV na ADC pinu (po děliči)
    delayMicroseconds(200);
  }
  float v_adc = (sum_mV / (float)BAT_SAMPLES) / 1000.0f; // V
  float v_bat = v_adc * deviderRatio;                    // přepočet přes dělič
  return v_bat;
}

// Fixed color bar (homogenní přes pushImage)
static void drawColorBar(float vmin, float vmax) {
  tft.fillRect(BAR_X-1, BAR_Y-1, BAR_W+2, BAR_H+2, TFT_BLACK);

  static uint16_t barCol[BAR_H];
  for (int yy = 0; yy < BAR_H; ++yy) {
    float t = 1.0f - (float)yy / (float)(BAR_H - 1); // top = max
    float val = vmin + t*(vmax - vmin);
    barCol[yy] = color565_from_value(val, vmin, vmax);
  }
  tft.setSwapBytes(USE_BYTE_SWAP);
  for (int x = 0; x < BAR_W; ++x) {
    tft.pushImage(BAR_X + x, BAR_Y, 1, BAR_H, barCol);
  }
  tft.drawRect(BAR_X, BAR_Y, BAR_W, BAR_H, TFT_WHITE);

  // Labels
  tft.setTextFont(2); tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TR_DATUM);
  char buf[24];
  snprintf(buf, sizeof(buf), "%.1f C", vmax); tft.drawString(buf, BAR_X - 2, BAR_Y);
  tft.setTextDatum(BR_DATUM);
  snprintf(buf, sizeof(buf), "%.1f C", vmin); tft.drawString(buf, BAR_X - 2, BAR_Y + BAR_H);
}

// Bottom info row (outside the heat picture → no flicker)
static void drawInfoRow(float* img) {
  float tMin=1e9f, tMax=-1e9f, sum=0.f;
  for (int i=0;i<192;i++){ float v=img[i]; if(v<tMin)tMin=v; if(v>tMax)tMax=v; sum+=v; }
  float avg=sum/192.f;

  // battery
  float vbat = readBatteryVoltage();

  tft.fillRect(INFO_X, INFO_Y, INFO_W, INFO_H, TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextFont(2);

  // left: temps
  tft.setTextDatum(TL_DATUM);
  char l1[64];
  snprintf(l1,sizeof(l1),"MLX90641 16x12  min=%.2f  max=%.2f  avg=%.2f", tMin, tMax, avg);
  tft.drawString(l1, INFO_X + 6, INFO_Y + 1);

  // right: battery
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextDatum(TR_DATUM);
  char l2[32];
  snprintf(l2,sizeof(l2),"VBAT=%.2f V", vbat);
  tft.drawString(l2, INFO_X + INFO_W - 6, INFO_Y + 1);
}

// Bilinear upscale 16x12 -> HM_W x HM_H directly to TFT
static void drawHeatmap(float* img) {
  float fmin= 1e9f, fmax=-1e9f;
  for (int i=0;i<192;i++){ float v=img[i]; if(v<fmin)fmin=v; if(v>fmax)fmax=v; }
  if (fmax - fmin < 0.5f) { fmin -= 0.25f; fmax += 0.25f; }
  vminEma = (1.0f-CLIM_ALPHA)*vminEma + CLIM_ALPHA*fmin;
  vmaxEma = (1.0f-CLIM_ALPHA)*vmaxEma + CLIM_ALPHA*fmax;
  if (vmaxEma - vminEma < 0.5f) { float mid=0.5f*(vminEma+vmaxEma); vminEma=mid-0.25f; vmaxEma=mid+0.25f; }

  tft.setSwapBytes(USE_BYTE_SWAP);

  const float sx = (16 - 1) / float(HM_W - 1);
  const float sy = (12 - 1) / float(HM_H - 1);

  for (int dy = 0; dy < HM_H; ++dy) {
    float fy = dy * sy;
    int y0 = (int)fy; int y1 = min(y0 + 1, 11);
    float wy = fy - y0;

    for (int dx = 0; dx < HM_W; ++dx) {
      float fx = dx * sx;
      int x0 = (int)fx; int x1 = min(x0 + 1, 15);
      float wx = fx - x0;

      float a = img[y0*16 + x0];
      float b = img[y0*16 + x1];
      float c = img[y1*16 + x0];
      float d = img[y1*16 + x1];

      float top = a + (b - a) * wx;
      float bot = c + (d - c) * wx;
      float t   = top + (bot - top) * wy;

      lineBuf[dx] = color565_from_value(t, vminEma, vmaxEma);
    }
    tft.pushImage(HM_X, HM_Y + dy, HM_W, 1, lineBuf);
  }

  // Crosshair at hottest pixel
  int idx=0; float maxv=img[0];
  for (int i=1;i<192;i++){ if (img[i]>maxv){ maxv=img[i]; idx=i; } }
  int py = idx / 16, px = idx % 16;
  int cx = (int)round(px * (HM_W - 1) / 15.0f);
  int cy = (int)round(py * (HM_H - 1) / 11.0f);

  int x0 = max(HM_X,           HM_X + cx - CROSS_SIZE);
  int x1 = min(HM_X + HM_W-1,  HM_X + cx + CROSS_SIZE);
  int y0 = max(HM_Y,           HM_Y + cy - CROSS_SIZE);
  int y1 = min(HM_Y + HM_H-1,  HM_Y + cy + CROSS_SIZE);

  tft.drawFastHLine(x0, HM_Y + cy, x1 - x0 + 1, CROSS_COLOR);
  tft.drawFastVLine(HM_X + cx, y0, y1 - y0 + 1, CROSS_COLOR);

  tft.setTextFont(2); tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  char buf[24]; snprintf(buf, sizeof(buf), "%.2f C", maxv);
  int lx = min(HM_X + HM_W - 60, HM_X + cx + 6);
  int ly = min(HM_Y + HM_H - 14, HM_Y + cy + 6);
  tft.drawString(buf, lx, ly);
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

  // --- Battery ADC setup ---
  analogReadResolution(12);                    // 12b (0..4095)
  analogSetPinAttenuation(BAT_PIN, ADC_11db);  // pro ~0..3.3V měření na ADC

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
}

void loop() {
  int st = MLX90641_GetFrameData(MLX_ADDR, gFrame);
  if (st < 0) { delay(40); return; }

  float Ta = MLX90641_GetTa(gFrame, &gParams);
  (void)Ta;
  MLX90641_CalculateTo(gFrame, &gParams, EMISSIVITY, Ta, gTo);

#if FLIP_X
  flipInputHorizontal(gTo);
#endif
#if FLIP_Y
  flipInputVertical(gTo);
#endif

#if AVERAGE_N > 0
  for (int i=0;i<192;i++){ gAcc[i] = (gAcc[i]*gAccCount + gTo[i])/(gAccCount+1); }
  gAccCount++;
  bool ready = (gAccCount >= AVERAGE_N);
  float* img = ready ? gAcc : gTo;
#else
  bool ready = true;
  float* img = gTo;
#endif

  if (ready) {
    drawHeatmap(img);
    drawColorBar(vminEma, vmaxEma);
    drawInfoRow(img);

#if AVERAGE_N > 0
    memset(gAcc, 0, sizeof(gAcc));
    gAccCount = 0;
#endif
  }

  delay(40); // ~16 Hz
}