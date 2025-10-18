/* ESPD-3.5" - used for automatic regulation of light and measurement of
 * CO2, temperature, humidity and light For Maker Faire
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
 * Used library:
 * TFT_eSPI - https://github.com/Bodmer/TFT_eSPI
 * TSL2561 https://github.com/adafruit/Adafruit_TSL2561/
 * SCD41 - https://github.com/sparkfun/SparkFun_SCD4x_Arduino_Library
 * SEN66 - https://github.com/Sensirion/arduino-i2c-sen66
 * Email:podpora@laskakit.cz
 * Web:laskakit.cz
*/


#include <WiFi.h>

// TFT Display
#include <TFT_eSPI.h>  // Hardware-specific library
#include <SPI.h>

// SCD41
#include "SparkFun_SCD4x_Arduino_Library.h"

// TSL2561
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>

// SEN66
#include <SensirionI2cSen66.h>

// BMP280
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

// SHT40
#include "Adafruit_SHT4x.h"

// LTR390
#include "Adafruit_LTR390.h"

// Fonts
#include "OpenSansSB_14px.h"
#include "OpenSansSB_20px.h"
#include "OpenSansSB_40px.h"
#include "OpenSansSB_60px.h"
#include "DSEG14Classic_100px_bold.h"
#include "DSEG14Classic_80px_bold.h"
#include "DSEG14Classic_60px_bold.h"


#include <TFT_eSPI.h>
#include <math.h>

TFT_eSPI tft = TFT_eSPI();

#define DEG2RAD 0.0174532925

// sensors enabled
bool tsl2561_en = true;
bool scd41_en = true;
bool sen66_en = true;
bool bmp280_en = true;
bool sht40_en = false;
bool ltr390_en = true;

// TSL2561
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);
uint16_t TSL2561_light = 0;
uint16_t TSL2561_broadband = 0;
uint16_t TSL2561_ir = 0;

// SCD41
SCD4x SCD41;
int SCD41_CO2 = 0;
float SCD41_temperature = 0.0;
int SCD41_humidity = 0;

// SEN66
SensirionI2cSen66 sensor;
static int16_t error;
#define NO_ERROR 0
float SEN66_massConcentrationPm1p0 = 0.0;
float SEN66_massConcentrationPm2p5 = 0.0;
float SEN66_massConcentrationPm4p0 = 0.0;
float SEN66_massConcentrationPm10p0 = 0.0;
float SEN66_humidity = 0.0;
float SEN66_temperature = 0.0;
float SEN66_vocIndex = 0.0;
float SEN66_noxIndex = 0.0;
uint16_t SEN66_CO2 = 0;

// BMP280
Adafruit_BMP280 bmp; // I2C
float BMP280_pressure = 0.0;

// SHT40
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
sensors_event_t SHT40_temperature, SHT40_humidity;

// LTR390
Adafruit_LTR390 ltr = Adafruit_LTR390();
uint16_t ltr390_uv = 0;

struct Gauge {
  int x, y, r;
  float value;        // aktuální hodnota 0–100
  int targetValue;    // cílová hodnota 0–100
  int realValue;      // původní hodnota 0–1000
  float lastValue;    // předchozí hodnota ručičky
  const char* label;
  const char* units;
};

const char* gaugeLabels[6] = {
  "Teplota",
  "Vlhkost",
  "CO2",
  "Pev. cast.",
  "Osvetleni",
  "UV"
};

const char* gaugeUnits[6] = {
  "degC",
  "%Rh",
  "ppm",
  "ug/m3",
  "lux",
  "-"
};

Gauge gauges[6]; // místo samostatných polí

void drawNeedle(Gauge& g, uint16_t color) {
  float angleDeg = map(g.value, 0, 100, 180, 360);
  float angleRad = angleDeg * DEG2RAD;
  int x2 = g.x + cos(angleRad) * (g.r - 12);
  int y2 = g.y + sin(angleRad) * (g.r - 12);
  tft.drawLine(g.x, g.y, x2, y2, color);
}

void drawGaugeStatic(Gauge& g) {
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  // Nadpis
  tft.setFreeFont(&OpenSansSB_20px);
  tft.drawString(g.label, g.x, g.y - g.r - 20);
  tft.setTextFont(2);

  // Polokruh
  for (int angle = 180; angle <= 360; angle++) {
    float rad = angle * DEG2RAD;
    int x1 = g.x + cos(rad) * g.r;
    int y1 = g.y + sin(rad) * g.r;
    tft.drawPixel(x1, y1, TFT_WHITE);
  }

  // Rysky
  for (int i = 180; i <= 360; i += 30) {
    float angle = i * DEG2RAD;
    int x1 = g.x + cos(angle) * (g.r - 10);
    int y1 = g.y + sin(angle) * (g.r - 10);
    int x2 = g.x + cos(angle) * g.r;
    int y2 = g.y + sin(angle) * g.r;
    tft.drawLine(x1, y1, x2, y2, TFT_WHITE);
  }

  // Min a max
  tft.setTextDatum(CL_DATUM);
  tft.drawString("MIN", g.x - g.r, g.y + 5);
  tft.setTextDatum(CR_DATUM);
  tft.drawString("MAX", g.x + g.r, g.y + 5);

}

void drawGaugeValue(const Gauge& g) {
  tft.setFreeFont(&OpenSansSB_20px); 
  char buf[10];
  sprintf(buf, "%d", g.realValue);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(buf, g.x, g.y + g.r - 25);
  tft.setTextFont(2);
  //sprintf(buf, "%s", g.units);
  tft.drawString(g.units, g.x, g.y + g.r - 5);
}

void setup() {

  Serial.begin(115200);
  delay(2000);

  Wire.begin(I2C_SDA, I2C_SCL);

  scd41_init();

  tsl2561_init();

  sen66_init();

  bmp280_init();

  sht40_init();

  ltr390_init();

  tft.init();
  tft.setRotation(0); // orientace na výšku (320x480)
  tft.fillScreen(TFT_BLACK);
  tft.setTextFont(2); // volitelně: jiný font

  int cols = 2;
  int rows = 3;
  int radius = 50;
  int xSpacing = 320 / cols;
  int ySpacing = 480 / rows;

  for (int i = 0; i < 6; i++) {
    int col = i % cols;
    int row = i / cols;

    gauges[i].x = xSpacing * col + xSpacing / 2;
    gauges[i].y = ySpacing * row + ySpacing / 2 + 25;
    gauges[i].r = radius;
    gauges[i].value = 0;
    gauges[i].targetValue = 0;
    gauges[i].realValue = 0;
    gauges[i].lastValue = 0;
    gauges[i].label = gaugeLabels[i];
    gauges[i].units = gaugeUnits[i];

    drawGaugeStatic(gauges[i]); // vykreslí rámeček, stupnici, texty
    drawNeedle(gauges[i], TFT_RED); // první ručička
  }
}

void loop() {
  static unsigned long lastUpdate = 0;
  static unsigned long lastDraw = 0;
  static unsigned long lastFullRefresh = 0;

  if (millis() - lastUpdate > 60000) {
    lastUpdate = millis();

    tft.fillScreen(TFT_BLACK);
    
    int cols = 2;
    int rows = 3;
    int radius = 50;
    int xSpacing = 320 / cols;
    int ySpacing = 480 / rows;

    for (int i = 0; i < 6; i++) {
      int col = i % cols;
      int row = i / cols;

      gauges[i].x = xSpacing * col + xSpacing / 2;
      gauges[i].y = ySpacing * row + ySpacing / 2 + 25;
      gauges[i].r = radius;
      gauges[i].value = 0;
      gauges[i].targetValue = 0;
      gauges[i].realValue = 0;
      gauges[i].lastValue = 0;
      gauges[i].label = gaugeLabels[i];
      gauges[i].units = gaugeUnits[i];

      drawGaugeStatic(gauges[i]); // vykreslí rámeček, stupnici, texty
      drawNeedle(gauges[i], TFT_RED); // první ručička
    }

  }

  if (millis() - lastFullRefresh > 3000) {
    lastFullRefresh = millis();

    if (tsl2561_en == true) {
      tsl2561_measure();
      gauges[4].realValue = TSL2561_light;                     // 0–1200
      gauges[4].targetValue = map(gauges[4].realValue, 0, 5000, 0, 100);  // 0–100 %
    }

    if (bmp280_en == true){
      bmp280_measure();
      gauges[5].realValue = BMP280_pressure;                     // 0–5000
      gauges[5].targetValue = map(gauges[5].realValue, 0, 1200, 0, 100);  // 0–100 %
    }

    if(ltr390_en == true){
      ltr390_measure();
      gauges[5].realValue = ltr390_uv;                     // 0-2300
      gauges[5].targetValue = map(gauges[5].realValue, 0, 2300, 0, 100);  // 0–100 %
    } 

    if (sht40_en == true)
    {
      sht40_measure();
      gauges[0].realValue = SHT40_temperature.temperature;                     // 50
      gauges[0].targetValue = map(gauges[0].realValue, 0, 50, 0, 100);  // 0–100 %

      gauges[1].realValue = SHT40_humidity.relative_humidity;                     // 100
      gauges[1].targetValue = map(gauges[1].realValue, 0, 100, 0, 100);  // 0–100 %
    }
    if (scd41_en == true) {
      scd41_measure();
      gauges[2].realValue = SCD41_CO2;                     // 0–5000
      gauges[2].targetValue = map(gauges[2].realValue, 0, 5000, 0, 100);  // 0–100 %
    }
    if (sen66_en == true){
      sen66_measure();
      gauges[0].realValue = SEN66_temperature;                     // 0–5000
      gauges[0].targetValue = map(gauges[0].realValue, 0, 50, 0, 100);  // 0–100 %

      gauges[1].realValue = SEN66_humidity;                     // 0–5000
      gauges[1].targetValue = map(gauges[1].realValue, 0, 100, 0, 100);  // 0–100 %

      gauges[2].realValue = SEN66_CO2;                     // 0–5000
      gauges[2].targetValue = map(gauges[2].realValue, 0, 3000, 0, 100);  // 0–100 %

      gauges[3].realValue = SEN66_massConcentrationPm10p0;                     // 0–5000
      gauges[3].targetValue = map(gauges[3].realValue, 0, 250, 0, 100);  // 0–100 %

    }
  }

  if (millis() - lastDraw > 50) {
    lastDraw = millis();

    for (int i = 0; i < 6; i++) {
      Gauge& g = gauges[i];

      // SMAŽ STAROU RUČIČKU
      drawNeedle(g, TFT_BLACK);

      // INTERPOLUJ NOVOU HODNOTU
      float diff = g.targetValue - g.value;
      if (abs(diff) > 0.5)
        g.value += diff * 0.1;
      else
        g.value = g.targetValue;

      // NAKRESLI NOVOU RUČIČKU
      drawNeedle(g, TFT_RED);

      // Zobrazit hodnotu pod budíkem
      drawGaugeValue(g);
    }
  }
}

void ltr390_init(){
    if ( ! ltr.begin() ) {
    Serial.println("ltr390 init");
    ltr390_en = false;
  }

  if(ltr390_en == true){
    ltr.setMode(LTR390_MODE_UVS);
    ltr.setGain(LTR390_GAIN_3);
    ltr.setResolution(LTR390_RESOLUTION_16BIT);
    ltr.setThresholds(100, 1000);
    ltr.configInterrupt(true, LTR390_MODE_UVS);
  }
}

void ltr390_measure(){
    if (ltr.newDataAvailable()) {
      ltr390_uv = ltr.readUVS();
      Serial.print("LTR390 [UV]: "); Serial.println(ltr390_uv);
  }
}

void sht40_init(){
    if (! sht4.begin()) {
    Serial.println("sht40 init");
    sht40_en = false;
  }

  if(sht40_en == true)
  {
    sht4.setPrecision(SHT4X_HIGH_PRECISION);
    sht4.setHeater(SHT4X_NO_HEATER);
  }
}

void sht40_measure(){
  if(sht40_en == true){
    sht4.getEvent(&SHT40_humidity, &SHT40_temperature);

    Serial.print("SHT40 [temperature]: "); Serial.println(SHT40_temperature.temperature);
    Serial.print("SHT40 [humidity]: "); Serial.println(SHT40_humidity.relative_humidity);
  }
}

void bmp280_init(){
  if (!bmp.begin(0x77)) {  // Adresa může být 0x76 nebo 0x77
    Serial.println("bmp280 init");
    bmp280_en = false;
  }
  if(bmp280_en == true)
  {
    bmp.setSampling(
    Adafruit_BMP280::MODE_NORMAL,
    Adafruit_BMP280::SAMPLING_X2,   // Teplota
    Adafruit_BMP280::SAMPLING_X16,  // Tlak
    Adafruit_BMP280::FILTER_X16,
    Adafruit_BMP280::STANDBY_MS_500
  );
  }
}
void bmp280_measure(){
  if(bmp280_en == true)
  {
    BMP280_pressure = bmp.readPressure() / 100.0F;   // hPa
    Serial.print("BMP280 [pressure]: "); Serial.println(BMP280_pressure);
  }
}

void sen66_init(){
  sensor.begin(Wire, SEN66_I2C_ADDR_6B);
  error = sensor.deviceReset();
  if (error != NO_ERROR) 
  {
    sen66_en = false;
    Serial.print("sen66 init");
  }
  if(sen66_en == true)
  {
    sensor.startContinuousMeasurement();
  }
}

void sen66_measure(){
  error = sensor.readMeasuredValues(
        SEN66_massConcentrationPm1p0, SEN66_massConcentrationPm2p5, SEN66_massConcentrationPm4p0,
        SEN66_massConcentrationPm10p0, SEN66_humidity, SEN66_temperature, SEN66_vocIndex, SEN66_noxIndex,
        SEN66_CO2);

        Serial.print("SEN66 [PM10]: "); Serial.println(SEN66_massConcentrationPm10p0);
        Serial.print("SEN66 [CO2]: "); Serial.println(SEN66_CO2);
        Serial.print("SEN66 [temperature]: "); Serial.println(SEN66_temperature);
        Serial.print("SEN66 [humidity]: "); Serial.println(SEN66_humidity);
        Serial.print("SEN66 [NOX index]: "); Serial.println(SEN66_noxIndex);
        Serial.print("SEN66 [VOC index]: "); Serial.println(SEN66_vocIndex);

        /*SEN66_humidity = 0.0;
        SEN66_temperature = 0.0;*/
        if(SEN66_massConcentrationPm10p0 == 6553.50)
        {
          SEN66_massConcentrationPm1p0 = 0.0;
          SEN66_massConcentrationPm2p5 = 0.0;
          SEN66_massConcentrationPm4p0 = 0.0;
          SEN66_massConcentrationPm10p0 = 0.0;
        }


        if(SEN66_CO2 == 65535){
          SEN66_vocIndex = 0.0;
          SEN66_noxIndex = 0.0;
          SEN66_CO2 = 0;
        }

}

void tsl2561_init() {
  if (!tsl.begin()) {
    tsl2561_en = false;
    Serial.println("tsl2561 init");
  }
  if (tsl2561_en == true) {
    tsl.enableAutoRange(true);                              // Automatický rozsah
    tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  // Kratší doba integrace
  }
}
void tsl2561_measure() {
  sensors_event_t event;
  tsl.getEvent(&event);
  //Serial.println("tsl2561_measure");
  if (event.light) {
    // Čtení aktuální intenzity světla
    TSL2561_light = event.light;
    Serial.print("TSL2561 [light]: "); Serial.println(TSL2561_light);
  }

  tsl.getLuminosity(&TSL2561_broadband, &TSL2561_ir);
  Serial.print("TSL2561 [visible + IR]: "); Serial.println(TSL2561_broadband);
  Serial.print("TSL2561 [IR]: "); Serial.println(TSL2561_ir);
}

void scd41_init() {
  if (SCD41.begin(true, false) == false) {
    scd41_en = false;
    Serial.println("scd41 init");
  }
}
void scd41_measure() {
  if (SCD41.readMeasurement())  // wait for a new data (approx 30s)
  {
    SCD41_CO2 = SCD41.getCO2();
    SCD41_temperature = SCD41.getTemperature();
    SCD41_humidity = SCD41.getHumidity();

    Serial.print("SCD41 [CO2]: "); Serial.println(SCD41_CO2);
    Serial.print("SCD41 [temperature]: "); Serial.println(SCD41_temperature);
    Serial.print("SCD41 [humidity]: "); Serial.println(SCD41_humidity);
  }
}



#if 0
// TFT display
TFT_eSPI tft = TFT_eSPI();
unsigned long lastTimeBotRan;
int updtDisplayDelay = 10000; // update the time each 10s

// SCD41
SCD4x SCD41;
int co2SCD41 = 0;
float tempSCD41 = 0.0;
int humSCD41 = 0;

// TSL2561
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);
int light = 0;

// What is connected
bool scd41_en = true;
bool tsl2561_en = true;

void setup()
{
  Serial.begin(115200);

  Wire.begin();

  scd41_init();

  sen54_init();

  tsl2561_init();

  displayInit();
}
void loop()
{ 

  if(tsl2561_en == true)
  {
    tsl2561_measure();
  }

  if(scd41_en == true)
  {
    scd41_measure();
  }
    
  //update display
  updtDisplay();
  
  delay(5000);
}

void scd41_init()
{
  if (SCD41.begin(true, false) == false)
  {
    scd41_en = false;
    Serial.println("scd41 init");
  }
}



void tsl2561_measure()
{
  sensors_event_t event;
  tsl.getEvent(&event);
  //Serial.println("tsl2561_measure");
  if (event.light) 
  {
    // Čtení aktuální intenzity světla
    light = event.light;
  }
}

void scd41_measure()
{
  if(SCD41.readMeasurement()) // wait for a new data (approx 30s)
  {
    //Serial.println();
    co2SCD41 = SCD41.getCO2();
    tempSCD41 = SCD41.getTemperature();
    humSCD41 = SCD41.getHumidity();
/*
    Serial.print("CO2(ppm):");
    Serial.print(co2SCD41);

    Serial.print("\tTemperature(C):");
    Serial.print(tempSCD41, 1);

    Serial.print("\tHumidity(%RH):");
    Serial.print(humSCD41);

    Serial.println();
*/
  } 
}

void updtDisplay()
{
  //Serial.println("updt display");

  tft.fillScreen(TFT_BLACK);
  //tft.setTextColor(TFT_WHITE);

  if(scd41_en == true)
  {
    tft.setTextColor(TFT_GREEN);
    tft.setCursor(10,330);
    tft.setFreeFont(&OpenSansSB_60px); 
    tft.print(String(co2SCD41)); // co2
    tft.setFreeFont(&OpenSansSB_40px); 
    tft.print(" ppm");
    tft.setTextColor(TFT_RED);
    tft.setCursor(10,400);
    tft.setFreeFont(&OpenSansSB_60px); 
    tft.print(String(tempSCD41, 1)); // temperature
    tft.setFreeFont(&OpenSansSB_40px);
    tft.print(" degC");
    tft.setTextColor(TFT_BLUE);
    tft.setCursor(10,470);
    tft.setFreeFont(&OpenSansSB_60px); 
    tft.print(String(humSCD41)); // humidity
    tft.setFreeFont(&OpenSansSB_40px);
    tft.print(" %Rh");
  }
  if(sen54_en == true)
  {
    tft.setFreeFont(&DSEG14_Classic_Bold_60); 
    tft.setTextColor(TFT_BLACK);
    tft.setCursor(240,10);
    tft.print(String(massConcentrationPm4p0)); // PM
    tft.setTextColor(TFT_RED);
    tft.setCursor(320,10);
    tft.print(String(ambientTemperature, 1)); // temperature
    tft.setTextColor(TFT_BLUE);
    tft.setCursor(400,10);
    tft.print(String(ambientHumidity, 0)); // humidity
  }
  
  //drawRectangles(Output, Input); // Update rectangles

  // Nastavení středu budíku a poloměru
  int centerX = tft.width() / 4;
  int centerY = tft.height() - 10;
  int radius = min(centerX, centerY) - 10;

  // Kreslení půlkruhu budíku
  drawHalfCircle(centerX, centerY-300, radius, TFT_WHITE);
  tft.setFreeFont(&OpenSansSB_14px);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(0, centerY-280);
  tft.println("MAX ");
  tft.println("osvetleni: ");
  tft.setFreeFont(&OpenSansSB_20px);
  tft.print(light_FullPWM);
  tft.setFreeFont(&OpenSansSB_14px);
  tft.print(" Lux");

  tft.setCursor(220, centerY-280);
  tft.println("MIN ");
  tft.setCursor(220, centerY-265);
  tft.println("osvetleni: ");
  tft.setCursor(220, centerY-245);
  tft.setFreeFont(&OpenSansSB_20px);
  tft.print(light_NoPWM);
  tft.setFreeFont(&OpenSansSB_14px);
  tft.print(" Lux");

  // Kreslení ručiček
  tft.setTextColor(TFT_YELLOW);
  int temp_targetLux = double((targetLux*180)/light_FullPWM);
  tft.setCursor(240, centerY-450);
  tft.println("AKTUALNI:");
  tft.setCursor(240, centerY-435);
  tft.print(String(Input,0));
  tft.print(" Lux");

  tft.setTextColor(TFT_RED);
  int temp_input_h = double((Input*180)/light_FullPWM);
  tft.setCursor(0, centerY-450);
  tft.println("CIL:");
  tft.setFreeFont(&OpenSansSB_14px);
  tft.print(targetLux);
  tft.print(" Lux");

  drawHand(centerX, centerY-300, radius - 10, temp_targetLux, TFT_RED);  // Ručička 1 - TARGET
  drawHand(centerX, centerY-300, radius - 20, temp_input_h, TFT_YELLOW);  // Ručička 2 - AKTUALNI
}

// Funkce pro kreslení půlkruhu
void drawHalfCircle(int x, int y, int radius, uint16_t color) {
  int segments = 180;  // Počet segmentů pro hladší kruh
  for (int i = 0; i <= segments; i++) {
    float angle = i * 3.14159265358979 / segments;
    int x1 = x + radius * cos(angle);
    int y1 = y - radius * sin(angle);
    int x2 = x + radius * cos(angle + 3.14159265358979 / segments);
    int y2 = y - radius * sin(angle + 3.14159265358979 / segments);
    tft.drawLine(x1, y1, x2, y2, color);
  }
}

// Funkce pro kreslení ručičky
void drawHand(int x, int y, int length, int angle, uint16_t color) {
  float radian = angle * 3.14159265358979 / 180;
  int x2 = x + length * cos(radian);
  int y2 = y - length * sin(radian);
  // Kreslení ručičky s tloušťkou 3 pixely
  for (int i = -1; i <= 1; i++) {
    for (int j = -1; j <= 1; j++) {
      tft.drawLine(x + i, y + j, x2 + i, y2 + j, color);
    }
  }
}




void drawRectangles(int output_h, int input_h) 
{

  // First rectangle
  int temp_output_h = ((output_h*200)/light_FullPWM);
  //Serial.print("Output H: "); Serial.println(temp_output_h);
  tft.fillRect(20, 5, 120, temp_output_h, TFT_YELLOW);
  tft.setFreeFont(&OpenSansSB_20px);
  tft.setTextColor(TFT_BLUE);
  tft.setCursor(40, 40);
  //tft.print("SET");
  tft.print(((output_h*100)/255));
  tft.print(" %");
  
  // Second rectangle
  int temp_input_h = double((input_h*200)/light_FullPWM);
  //Serial.print("Input H: "); Serial.println(temp_input_h);
  tft.fillRect(180, 5, 120, temp_input_h, TFT_GREEN);
  tft.setFreeFont(&OpenSansSB_20px);
  tft.setTextColor(TFT_BLUE);
  tft.setCursor(195, 40);
  //tft.print("GET");
  tft.print(input_h);
  tft.print(" Lux");

  int temp_targetLux = double((targetLux*200)/light_FullPWM);
  //Serial.print("Target LUX H: "); Serial.println(temp_targetLux);
  tft.fillRect(160, temp_targetLux, 320, 5, TFT_RED);
  tft.setFreeFont(&OpenSansSB_20px);
  tft.setTextColor(TFT_RED);
  tft.setCursor(195, temp_targetLux-20);
  tft.print("Muj cil");
  tft.setCursor(195, temp_targetLux+40);
  tft.print(targetLux);
  tft.print(" Lux");


}

// replace the czech characters 
String fixCzechCharacters(String data) 
{
	data.replace("Ě", "E");
	data.replace("Š", "S");
	data.replace("Č", "C");
	data.replace("Ř", "R");
	data.replace("Ž", "Z");
	data.replace("Ý", "Y");
	data.replace("Á", "A");
	data.replace("Í", "I");
	data.replace("É", "E");
	data.replace("Ů", "U");
	data.replace("Ú", "U");
	data.replace("Ď", "D");
	data.replace("Ť", "T");
	data.replace("Ň", "N");
	data.replace("ě", "e");
	data.replace("š", "s");
	data.replace("č", "c");
	data.replace("ř", "r");
	data.replace("ž", "z");
	data.replace("ý", "y");
	data.replace("á", "a");
	data.replace("í", "i");
	data.replace("é", "e");
	data.replace("ů", "u");
	data.replace("ú", "u");
	data.replace("ď", "d");
	data.replace("ť", "t");
	data.replace("ň", "n");
	data.replace("°C", "");
	return data;
}

#endif
