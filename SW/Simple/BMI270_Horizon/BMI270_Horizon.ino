/* 
 * This example code is used for LaskaKit ESPD-3.5 ESP32 3.5 TFT ILI9488 Touch v3.0 https://www.laskakit.cz/laskakit-espd-35-esp32-3-5-tft-ili9488-touch/
 * with Inertial Measurement Unit BMI270 on board
 * ESPD-3.5 board reads the gyroscope values from the BMI270 sensor and continuously draw horizon on display
 * 
 * How to steps:
 * 1. Copy file Setup303_ILI9488_ESPD-3_5_v3.h from https://github.com/LaskaKit/ESPD-35/tree/main/SW to Arduino/libraries/TFT_eSPI/User_Setups/  
 * 2. in Arduino/libraries/TFT_eSPI/User_Setup_Select.h 
      a. comment: #include <User_Setup.h>
      b. add: #include <User_Setups/Setup303_ILI9488_ESPD-3_5_v3.h>  // Setup file for LaskaKit ESPD-3.5" 320x480, ILI9488 V3
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
 * Libraries: SparkFun_BMI270_Arduino_Library
 *            TFT eSPI
 * Email:podpora@laskakit.cz
 * Web:laskakit.cz
*/

#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_BMI270_Arduino_Library.h>
#include <TFT_eSPI.h>
#include <math.h>

// Objects
BMI270 bmi270;
TFT_eSPI tft = TFT_eSPI();

// Screen constants
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320
#define CENTER_X (SCREEN_WIDTH / 2)
#define CENTER_Y (SCREEN_HEIGHT / 2)
#define SCALE 1.5
#define I2C_ADDRESS 0x68

// Calibration offsets
float rollOffset = 0.0;
float pitchOffset = 0.0;

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  while(bmi270.beginI2C(I2C_ADDRESS) != BMI2_OK) {
    Serial.println("Error: BMI270 not connected, check wiring and I2C address!");
    delay(1000);
  }
  tft.init();
  tft.setRotation(1);

  // Perform initial calibration
  bmi270.getSensorData();
  float ax = bmi270.data.accelX;
  float ay = bmi270.data.accelY;
  float az = bmi270.data.accelZ;
  rollOffset = atan2(ay, az) * 180.0 / PI;
  pitchOffset = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0 / PI;
  Serial.println("Calibration Done.");
}

void drawHorizon(float pitch, float roll) {
  float pitchOffsetDraw = pitch * (SCREEN_HEIGHT / 90.0);
  float cosR = cos(roll * PI / 180.0);
  float sinR = sin(roll * PI / 180.0);

  for (int x = -CENTER_X; x <= CENTER_X; x += 2) {
    float y = pitchOffsetDraw + (x * sinR / cosR);
    if (y < CENTER_Y) {
      tft.drawLine(CENTER_X + x, 0, CENTER_X + x, CENTER_Y + y, TFT_BLUE);
      tft.drawLine(CENTER_X + x, CENTER_Y + y, CENTER_X + x, SCREEN_HEIGHT, TFT_ORANGE);
    } else {
      tft.drawLine(CENTER_X + x, 0, CENTER_X + x, CENTER_Y + y, TFT_BLUE);
      tft.drawLine(CENTER_X + x, CENTER_Y + y, CENTER_X + x, SCREEN_HEIGHT, TFT_ORANGE);
    }
  }
  tft.drawRect(CENTER_X-30, CENTER_Y-3, 60, 6, TFT_WHITE);
}

void loop() {
  bmi270.getSensorData();
  float ax = bmi270.data.accelX;
  float ay = bmi270.data.accelY;
  float az = bmi270.data.accelZ;

  float roll = atan2(ay, az) * 180.0 / PI - rollOffset;
  float pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0 / PI - pitchOffset;

  // int start = millis();
  drawHorizon(pitch, roll);
  // Serial.println((millis() - start));
  delay(10);
}
