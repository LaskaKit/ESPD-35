/* ESPD-3.5" - Sensor Stack for Maker Faire
 * Measurement of temperature, humidity, CO2, particulate matter, light and UV
 * shown as six animated ring gauges.
 * Written by chiptron.cz for laskakit.cz (2023)
 *
 * How to steps:
 * 1. Copy file from https://github.com/LaskaKit/ESPD-35/tree/main/SW to Arduino/libraries/TFT_eSPI/User_Setups/
 *    - for version v2.3 and before:  Setup300_ILI9488_ESPD-3_5_v2.h
 *    - for version v3 and above:     Setup303_ILI9488_ESPD-3_5_v3.h
 * 2. in Arduino/libraries/TFT_eSPI/User_Setup_Select.h
      a. comment: #include <User_Setup.h>
      b. add:
          - for version v2.3 and before:  #include <User_Setups/Setup300_ILI9488_ESPD-3_5_v2.h>  // Setup file for LaskaKit ESPD-3.5" 320x480, ILI9488
          - for version v3 and above:     #include <User_Setups/Setup303_ILI9488_ESPD-3_5_v3.h>  // Setup file for LaskaKit ESPD-3.5" 320x480, ILI9488 V3
 *
 * Board constants:
      TFT_BL          - LED back-light use: analogWrite(TFT_BL, TFT_BL_PWM);
      POWER_OFF_PIN   - Pull LOW to switch board off
      TOUCH_INT       - Touch interrupt pin
    * I2C (µŠup and devices (only from v3)):
      I2C_SDA         - Data pin
      I2C_SCL         - Clock pin
 *
 * Used library:
 * TFT_eSPI (>= 2.5) - https://github.com/Bodmer/TFT_eSPI
 * TSL2561 https://github.com/adafruit/Adafruit_TSL2561/
 * SCD41 - https://github.com/sparkfun/SparkFun_SCD4x_Arduino_Library
 * SEN66 - https://github.com/Sensirion/arduino-i2c-sen66
 * BMP280 - https://github.com/adafruit/Adafruit_BMP280_Library
 * SHT40 - https://github.com/adafruit/Adafruit_SHT4X
 * LTR390 - https://github.com/adafruit/Adafruit_LTR390
 * Email:podpora@laskakit.cz
 * Web:laskakit.cz
*/

// TFT Display
#include <TFT_eSPI.h>  // Hardware-specific library
#include <SPI.h>
#include <math.h>

// SCD41
#include "SparkFun_SCD4x_Arduino_Library.h"

// TSL2561
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>

// SEN66
#include <SensirionI2cSen66.h>

// BMP280
#include <Adafruit_BMP280.h>

// SHT40
#include "Adafruit_SHT4x.h"

// LTR390
#include "Adafruit_LTR390.h"

// Fonts
#include "OpenSansSB_14px.h"
#include "OpenSansSB_20px.h"
#include "OpenSansSB_40px.h"

// Typy budíků (v hlavičce kvůli automatickým prototypům Arduino IDE)
#include "gauge.h"

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite valueSpr = TFT_eSprite(&tft);  // hodnota + jednotky uvnitř budíku (bez blikání)

// ---------------------------------------------------------------------------
// Barvy
// ---------------------------------------------------------------------------
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

const uint16_t C_BG     = rgb(8, 10, 14);
const uint16_t C_CARD   = rgb(24, 29, 38);
const uint16_t C_TRACK  = rgb(52, 60, 74);
const uint16_t C_DIM    = rgb(140, 150, 168);
const uint16_t C_TEXT   = TFT_WHITE;
const uint16_t C_ACCENT = rgb(230, 32, 52);   // LaskaKit červená

const uint16_t C_GREEN  = rgb(46, 204, 113);
const uint16_t C_LIME   = rgb(163, 217, 72);
const uint16_t C_YELLOW = rgb(241, 196, 15);
const uint16_t C_ORANGE = rgb(243, 140, 40);
const uint16_t C_RED    = rgb(231, 76, 60);
const uint16_t C_BLUE   = rgb(64, 156, 230);
const uint16_t C_CYAN   = rgb(26, 188, 196);
const uint16_t C_VIOLET = rgb(165, 105, 220);
const uint16_t C_GREY   = rgb(110, 120, 135);

// ---------------------------------------------------------------------------
// Rozložení (320x480 na výšku)
// ---------------------------------------------------------------------------
#define HEADER_H    44
#define TILE_W      154
#define TILE_H      140
#define TILE_GAP    4
#define TILE_R      12   // zaoblení karty

#define RING_R      63   // vnější poloměr prstence
#define RING_IR     54   // vnitřní poloměr prstence
#define RING_CY     85   // střed prstence od horního okraje karty
#define ARC_START   45   // TFT_eSPI: 0° = 6 hodin, po směru hodinových ručiček
#define ARC_END     315

#define SPR_W       94   // sprite s hodnotou musí zůstat uvnitř prstence
#define SPR_H       46

#define MEASURE_INTERVAL  3000
#define FRAME_INTERVAL    25

// ---------------------------------------------------------------------------
// Senzory
// ---------------------------------------------------------------------------
bool tsl2561_en = true;
bool scd41_en = true;
bool sen66_en = true;
bool bmp280_en = true;
bool sht40_en = true;
bool ltr390_en = true;

// TSL2561
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);
float TSL2561_light = NAN;

// SCD41
SCD4x SCD41;
float SCD41_CO2 = NAN;
float SCD41_temperature = NAN;
float SCD41_humidity = NAN;

// SEN66
SensirionI2cSen66 sensor;
static int16_t error;
#define NO_ERROR 0
float SEN66_massConcentrationPm1p0 = NAN;
float SEN66_massConcentrationPm2p5 = NAN;
float SEN66_massConcentrationPm4p0 = NAN;
float SEN66_massConcentrationPm10p0 = NAN;
float SEN66_humidity = NAN;
float SEN66_temperature = NAN;
float SEN66_vocIndex = NAN;
float SEN66_noxIndex = NAN;
float SEN66_CO2 = NAN;

// BMP280
Adafruit_BMP280 bmp;  // I2C
float BMP280_pressure = NAN;

// SHT40
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
float SHT40_temperature = NAN;
float SHT40_humidity = NAN;

// LTR390
Adafruit_LTR390 ltr = Adafruit_LTR390();
// Citlivost dle datasheetu: 2300 counts/UVI při gain 18x a 20 bit (400 ms).
// Pro gain 18x a 18 bit (100 ms) je to 2300 / 4 = 575 counts/UVI.
#define LTR390_UV_SENSITIVITY 575.0
float LTR390_uvi = NAN;

// ---------------------------------------------------------------------------
// Budíky
// ---------------------------------------------------------------------------


const Level LV_TEMP[] = { { 18, C_BLUE, "Chladno" }, { 26, C_GREEN, "Prijemne" }, { 30, C_ORANGE, "Teplo" }, { INFINITY, C_RED, "Horko" } };
const Level LV_HUM[]  = { { 30, C_ORANGE, "Sucho" }, { 60, C_GREEN, "Optimum" }, { 70, C_YELLOW, "Vlhko" }, { INFINITY, C_BLUE, "Mokro" } };
const Level LV_CO2[]  = { { 800, C_GREEN, "Vyborne" }, { 1000, C_LIME, "Dobre" }, { 1500, C_YELLOW, "Vyvetrat" }, { 2000, C_ORANGE, "Spatne" }, { INFINITY, C_RED, "Kriticke" } };
const Level LV_PM[]   = { { 10, C_GREEN, "Vyborne" }, { 20, C_LIME, "Dobre" }, { 25, C_YELLOW, "Zhorsene" }, { 50, C_ORANGE, "Spatne" }, { INFINITY, C_RED, "Kriticke" } };
const Level LV_LUX[]  = { { 50, C_VIOLET, "Tma" }, { 300, C_CYAN, "Sero" }, { 1000, C_YELLOW, "Svetlo" }, { INFINITY, C_ORANGE, "Jasno" } };
const Level LV_UV[]   = { { 3, C_GREEN, "Nizky" }, { 6, C_YELLOW, "Stredni" }, { 8, C_ORANGE, "Vysoky" }, { 11, C_RED, "Velmi vys." }, { INFINITY, C_VIOLET, "Extremni" } };

#define LEVELS(a) a, sizeof(a) / sizeof(a[0])



Gauge gauges[6] = {
  { "TEPLOTA",      "C",     0,   40,    1, false, LEVELS(LV_TEMP) },
  { "VLHKOST",      "%",     0,   100,   0, false, LEVELS(LV_HUM) },
  { "CO2",          "ppm",   400, 3000,  0, false, LEVELS(LV_CO2) },
  { "PRACH PM2.5",  "ug/m3", 0,   100,   1, false, LEVELS(LV_PM) },
  { "OSVETLENI",    "lux",   1,   10000, 0, true,  LEVELS(LV_LUX) },
  { "UV INDEX",     "UVI",   0,   12,    1, false, LEVELS(LV_UV) },
};

enum { G_TEMP, G_HUM, G_CO2, G_PM, G_LUX, G_UV };

const Level* levelFor(const Gauge& g, float v) {
  for (uint8_t i = 0; i < g.nLevels; i++)
    if (v < g.levels[i].upTo) return &g.levels[i];
  return &g.levels[g.nLevels - 1];
}

float fractionFor(const Gauge& g, float v) {
  float f;
  if (g.logScale)
    f = (log10f(max(v, g.minV)) - log10f(g.minV)) / (log10f(g.maxV) - log10f(g.minV));
  else
    f = (v - g.minV) / (g.maxV - g.minV);
  return constrain(f, 0.0f, 1.0f);
}

void formatValue(const Gauge& g, float v, char* buf, size_t len) {
  if (isnan(v)) {
    snprintf(buf, len, "--");
    return;
  }
  uint8_t dec = (fabsf(v) >= 100) ? 0 : g.decimals;
  snprintf(buf, len, "%.*f", dec, v);
}

void drawHeader() {
  tft.fillRect(0, 0, tft.width(), HEADER_H, C_BG);
  tft.fillSmoothRoundRect(10, 12, 5, 22, 2, C_ACCENT, C_BG);

  tft.setFreeFont(&OpenSansSB_20px);
  tft.setTextColor(C_TEXT);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("Sensor Stack", 24, 22);

  tft.setFreeFont(&OpenSansSB_14px);
  tft.setTextColor(C_DIM);
  tft.setTextDatum(MR_DATUM);
  tft.drawString("laskakit.cz", tft.width() - 10, 14);
  tft.drawString("chiptron.cz", tft.width() - 10, 31);
}

// Blikne tečkou v hlavičce při každém měření
void drawHeartbeat(uint16_t color) {
  tft.setFreeFont(&OpenSansSB_14px);
  int w = max(tft.textWidth("laskakit.cz"), tft.textWidth("chiptron.cz"));
  int x = tft.width() - 10 - w - 10;
  tft.fillSmoothCircle(x, 22, 3, color, C_BG);
}

void drawGaugeStatic(Gauge& g) {
  tft.fillSmoothRoundRect(g.x, g.y, TILE_W, TILE_H, TILE_R, C_CARD, C_BG);

  tft.setFreeFont(&OpenSansSB_14px);
  tft.setTextColor(C_DIM);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(g.label, g.x + 12, g.y + 8);

  tft.drawSmoothArc(g.x + TILE_W / 2, g.y + RING_CY, RING_R, RING_IR, ARC_START, ARC_END, C_TRACK, C_CARD, true);

  g.drawnAngle = ARC_START;
  g.drawnColor = C_TRACK;
  g.drawnText[0] = '\0';
  g.drawnStatus = nullptr;
}

void drawRing(Gauge& g, int16_t angle, uint16_t color) {
  int cx = g.x + TILE_W / 2;
  int cy = g.y + RING_CY;

  // zbytek stupnice, pak barevná výseč přes něj (kulatý konec překryje začátek stopy)
  if (angle < ARC_END)
    tft.drawSmoothArc(cx, cy, RING_R, RING_IR, angle, ARC_END, C_TRACK, C_CARD, true);
  if (angle > ARC_START)
    tft.drawSmoothArc(cx, cy, RING_R, RING_IR, ARC_START, angle, color, C_CARD, true);

  g.drawnAngle = angle;
  g.drawnColor = color;
}

void drawValue(Gauge& g, const char* text, uint16_t color) {
  valueSpr.fillSprite(C_CARD);

  // velká hodnota, pokud se nevejde, menší písmo
  valueSpr.setFreeFont(&OpenSansSB_40px);
  if (valueSpr.textWidth(text) > SPR_W)
    valueSpr.setFreeFont(&OpenSansSB_20px);
  valueSpr.setTextColor(color);
  valueSpr.setTextDatum(MC_DATUM);
  valueSpr.drawString(text, SPR_W / 2, 16);

  // jednotky, "C" = stupně Celsia (font nemá znak °)
  valueSpr.setFreeFont(&OpenSansSB_14px);
  valueSpr.setTextColor(C_DIM);
  if (strcmp(g.units, "C") == 0) {
    int w = valueSpr.textWidth("C");
    valueSpr.drawString("C", SPR_W / 2 + 3, 39);
    valueSpr.drawCircle(SPR_W / 2 + 3 - w / 2 - 4, 35, 2, C_DIM);
  } else {
    valueSpr.drawString(g.units, SPR_W / 2, 39);
  }

  valueSpr.pushSprite(g.x + TILE_W / 2 - SPR_W / 2, g.y + RING_CY - SPR_H / 2);
}

void drawStatus(Gauge& g, const char* status, uint16_t color) {
  int cx = g.x + TILE_W / 2;
  int cy = g.y + RING_CY;
  tft.fillRect(cx - 35, cy + 42, 70, 13, C_CARD);
  tft.setFreeFont(&OpenSansSB_14px);
  tft.setTextColor(color);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(status, cx, cy + 47);
  g.drawnStatus = status;
}

// Animace + překreslení jen toho, co se změnilo
void updateGauge(Gauge& g) {
  bool valid = !isnan(g.value);
  const Level* lvl = valid ? levelFor(g, g.value) : nullptr;
  uint16_t color = valid ? lvl->color : C_GREY;

  float target = valid ? fractionFor(g, g.value) : 0.0f;
  float diff = target - g.pos;
  if (fabsf(diff) > 0.002f)
    g.pos += diff * 0.15f;
  else
    g.pos = target;

  int16_t angle = ARC_START + lroundf(g.pos * (ARC_END - ARC_START));
  if (angle != g.drawnAngle || color != g.drawnColor)
    drawRing(g, angle, color);

  char text[12];
  formatValue(g, g.value, text, sizeof(text));
  if (strcmp(text, g.drawnText) != 0) {
    drawValue(g, text, valid ? C_TEXT : C_GREY);
    strcpy(g.drawnText, text);
  }

  const char* status = valid ? lvl->name : "bez dat";
  if (status != g.drawnStatus)
    drawStatus(g, status, color);
}

void drawScreen() {
  tft.fillScreen(C_BG);
  drawHeader();
  for (int i = 0; i < 6; i++) {
    gauges[i].x = TILE_GAP + (i % 2) * (TILE_W + TILE_GAP);
    gauges[i].y = HEADER_H + 2 + (i / 2) * (TILE_H + TILE_GAP);
    gauges[i].value = NAN;
    gauges[i].pos = 0;
    drawGaugeStatic(gauges[i]);
    updateGauge(gauges[i]);
  }
}

// Vrátí první platnou hodnotu (preferovaný senzor první)
float pick(float a, float b, float c = NAN) {
  if (!isnan(a)) return a;
  if (!isnan(b)) return b;
  return c;
}

void measureAll() {
  if (tsl2561_en) tsl2561_measure();
  if (bmp280_en) bmp280_measure();
  if (ltr390_en) ltr390_measure();
  if (sht40_en) sht40_measure();
  if (scd41_en) scd41_measure();
  if (sen66_en) sen66_measure();

  // SEN66 je hlavní senzor, ostatní slouží jako záloha
  gauges[G_TEMP].value = pick(SEN66_temperature, SHT40_temperature, SCD41_temperature);
  gauges[G_HUM].value  = pick(SEN66_humidity, SHT40_humidity, SCD41_humidity);
  gauges[G_CO2].value  = pick(SEN66_CO2, SCD41_CO2);
  gauges[G_PM].value   = SEN66_massConcentrationPm2p5;
  gauges[G_LUX].value  = TSL2561_light;
  gauges[G_UV].value   = LTR390_uvi;
}

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(0);  // orientace na výšku (320x480)
  valueSpr.setColorDepth(16);
  valueSpr.createSprite(SPR_W, SPR_H);
  drawScreen();

  Wire.begin(I2C_SDA, I2C_SCL);

  scd41_init();
  tsl2561_init();
  sen66_init();
  bmp280_init();
  sht40_init();
  ltr390_init();
}

void loop() {
  static unsigned long lastMeasure = 0;
  static unsigned long lastFrame = 0;
  static unsigned long heartbeatOff = 0;
  static bool firstMeasure = true;

  if (firstMeasure || millis() - lastMeasure > MEASURE_INTERVAL) {
    firstMeasure = false;
    lastMeasure = millis();
    drawHeartbeat(C_GREEN);
    heartbeatOff = millis() + 250;
    measureAll();
  }

  if (heartbeatOff && (long)(millis() - heartbeatOff) >= 0) {
    heartbeatOff = 0;
    drawHeartbeat(C_BG);
  }

  if (millis() - lastFrame > FRAME_INTERVAL) {
    lastFrame = millis();
    for (int i = 0; i < 6; i++)
      updateGauge(gauges[i]);
  }
}

// ---------------------------------------------------------------------------
// LTR390 - UV
// ---------------------------------------------------------------------------
void ltr390_init() {
  if (!ltr.begin()) {
    Serial.println("ltr390 init");
    ltr390_en = false;
    return;
  }
  ltr.setMode(LTR390_MODE_UVS);
  ltr.setGain(LTR390_GAIN_18);
  ltr.setResolution(LTR390_RESOLUTION_18BIT);
}

void ltr390_measure() {
  if (ltr.newDataAvailable()) {
    uint32_t uvs = ltr.readUVS();
    LTR390_uvi = uvs / LTR390_UV_SENSITIVITY;
    Serial.print("LTR390 [UVS]: "); Serial.print(uvs);
    Serial.print("  [UVI]: "); Serial.println(LTR390_uvi);
  }
}

// ---------------------------------------------------------------------------
// SHT40 - teplota a vlhkost
// ---------------------------------------------------------------------------
void sht40_init() {
  if (!sht4.begin()) {
    Serial.println("sht40 init");
    sht40_en = false;
    return;
  }
  sht4.setPrecision(SHT4X_HIGH_PRECISION);
  sht4.setHeater(SHT4X_NO_HEATER);
}

void sht40_measure() {
  sensors_event_t humidity, temperature;
  if (sht4.getEvent(&humidity, &temperature)) {
    SHT40_temperature = temperature.temperature;
    SHT40_humidity = humidity.relative_humidity;
    Serial.print("SHT40 [temperature]: "); Serial.println(SHT40_temperature);
    Serial.print("SHT40 [humidity]: "); Serial.println(SHT40_humidity);
  }
}

// ---------------------------------------------------------------------------
// BMP280 - tlak (jen do sériové linky)
// ---------------------------------------------------------------------------
void bmp280_init() {
  // Adresa může být 0x76 nebo 0x77
  if (!bmp.begin(0x77) && !bmp.begin(0x76)) {
    Serial.println("bmp280 init");
    bmp280_en = false;
    return;
  }
  bmp.setSampling(
    Adafruit_BMP280::MODE_NORMAL,
    Adafruit_BMP280::SAMPLING_X2,   // Teplota
    Adafruit_BMP280::SAMPLING_X16,  // Tlak
    Adafruit_BMP280::FILTER_X16,
    Adafruit_BMP280::STANDBY_MS_500);
}

void bmp280_measure() {
  BMP280_pressure = bmp.readPressure() / 100.0F;  // hPa
  Serial.print("BMP280 [pressure]: "); Serial.println(BMP280_pressure);
}

// ---------------------------------------------------------------------------
// SEN66 - PM, teplota, vlhkost, VOC, NOx, CO2
// ---------------------------------------------------------------------------
void sen66_init() {
  sensor.begin(Wire, SEN66_I2C_ADDR_6B);
  error = sensor.deviceReset();
  if (error != NO_ERROR) {
    sen66_en = false;
    Serial.println("sen66 init");
    return;
  }
  delay(1200);  // po resetu senzor potřebuje čas, jinak start měření selže
  error = sensor.startContinuousMeasurement();
  if (error != NO_ERROR) {
    sen66_en = false;
    Serial.println("sen66 start");
  }
}

void sen66_measure() {
  float pm1, pm2p5, pm4, pm10, hum, temp, voc, nox;
  uint16_t co2;
  error = sensor.readMeasuredValues(pm1, pm2p5, pm4, pm10, hum, temp, voc, nox, co2);
  if (error != NO_ERROR) {
    Serial.println("sen66 read");
    return;
  }

  // 0xFFFF / 0x7FFF = hodnota zatím není k dispozici (po startu měření)
  bool pmValid = pm10 < 6553.0;
  SEN66_massConcentrationPm1p0 = pmValid ? pm1 : NAN;
  SEN66_massConcentrationPm2p5 = pmValid ? pm2p5 : NAN;
  SEN66_massConcentrationPm4p0 = pmValid ? pm4 : NAN;
  SEN66_massConcentrationPm10p0 = pmValid ? pm10 : NAN;
  SEN66_humidity = (hum < 327.0) ? hum : NAN;
  SEN66_temperature = (temp < 163.0) ? temp : NAN;
  SEN66_vocIndex = (voc < 3276.0) ? voc : NAN;
  SEN66_noxIndex = (nox < 3276.0) ? nox : NAN;
  SEN66_CO2 = (co2 != 0xFFFF && co2 != 0) ? co2 : NAN;

  Serial.print("SEN66 [PM2.5]: "); Serial.println(SEN66_massConcentrationPm2p5);
  Serial.print("SEN66 [PM10]: "); Serial.println(SEN66_massConcentrationPm10p0);
  Serial.print("SEN66 [CO2]: "); Serial.println(SEN66_CO2);
  Serial.print("SEN66 [temperature]: "); Serial.println(SEN66_temperature);
  Serial.print("SEN66 [humidity]: "); Serial.println(SEN66_humidity);
  Serial.print("SEN66 [NOX index]: "); Serial.println(SEN66_noxIndex);
  Serial.print("SEN66 [VOC index]: "); Serial.println(SEN66_vocIndex);
}

// ---------------------------------------------------------------------------
// TSL2561 - osvětlení
// ---------------------------------------------------------------------------
void tsl2561_init() {
  if (!tsl.begin()) {
    tsl2561_en = false;
    Serial.println("tsl2561 init");
    return;
  }
  tsl.enableAutoRange(true);                              // Automatický rozsah
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  // Kratší doba integrace
}

void tsl2561_measure() {
  sensors_event_t event;
  if (!tsl.getEvent(&event)) {
    // getEvent vrací false při saturaci (light = 65536)
    Serial.println("TSL2561 saturated");
    TSL2561_light = 65536;
    return;
  }
  TSL2561_light = event.light;
  Serial.print("TSL2561 [light]: "); Serial.println(TSL2561_light);
}

// ---------------------------------------------------------------------------
// SCD41 - CO2, teplota, vlhkost
// ---------------------------------------------------------------------------
void scd41_init() {
  if (SCD41.begin(true, false) == false) {
    scd41_en = false;
    Serial.println("scd41 init");
  }
}

void scd41_measure() {
  if (SCD41.readMeasurement())  // nová data cca každých 5 s
  {
    SCD41_CO2 = SCD41.getCO2();
    SCD41_temperature = SCD41.getTemperature();
    SCD41_humidity = SCD41.getHumidity();

    Serial.print("SCD41 [CO2]: "); Serial.println(SCD41_CO2);
    Serial.print("SCD41 [temperature]: "); Serial.println(SCD41_temperature);
    Serial.print("SCD41 [humidity]: "); Serial.println(SCD41_humidity);
  }
}
