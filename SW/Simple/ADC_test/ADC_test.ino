/* 
 * ADC test for LaskaKit ESPD-3.5" 320x480, ILI9488 
 * example from TFT_eSPI library is used
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
 * Email:podpora@laskakit.cz
 * Web:laskakit.cz
*/

#include <Arduino.h>
#include <TFT_eSPI.h>               // Hardware-specific library
#include <SPI.h>

// TFT SPI
#define TFT_BL_PWM 255 // Backlight brightness 0-255
#define TFT_DISPLAY_RESOLUTION_X 480
#define TFT_DISPLAY_RESOLUTION_Y 320

TFT_eSPI tft = TFT_eSPI(); // Invoke custom library with default width and height

void displayInit() {
	tft.init();
	tft.setRotation(1);
	tft.fillScreen(TFT_BLACK);
}

void setup() {
  displayInit();
	analogWrite(TFT_BL, TFT_BL_PWM);      // Set brightness of backlight
}

void loop() {
	tft.setTextSize(1);
	tft.setTextFont(4);
	tft.setTextColor(TFT_WHITE, TFT_BLACK);
	tft.setTextDatum(MC_DATUM);
  tft.drawString("Battery voltage is " + String(analogReadMilliVolts(BAT_PIN) * deviderRatio / 1000) + " V", TFT_DISPLAY_RESOLUTION_X / 2, TFT_DISPLAY_RESOLUTION_Y / 2);
  delay(100);
}