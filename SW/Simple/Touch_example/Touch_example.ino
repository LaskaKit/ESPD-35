/* 
 * Touch test for LaskaKit ESPD-3.5" 320x480, ILI9488 https://www.laskakit.cz/laskakit-espd-35-esp32-3-5-tft-ili9488-touch/
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
 * Touch: 
 * Chip used in board is FT5436, library: https://github.com/DustinWatts/FT6236
 * Just changed CHIPID and VENDID
 * Library is included in the project so it does not need to be downloaded
 *
 * Email:podpora@laskakit.cz
 * Web:laskakit.cz
*/

#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>
#include <Arduino.h>
#include "FT6236.h"

// TFT SPI
#define TFT_BL_PWM 255 // Backlight brightness 0-255

FT6236 ts = FT6236(480, 320);
TFT_eSPI tft = TFT_eSPI(); // Invoke custom library with default width and height

void displayInit() {
	tft.init();
	tft.setRotation(1);
	tft.fillScreen(TFT_BLACK);
}

void setup(void) {
  Serial.begin(115200);
	Wire.begin(I2C_SDA, I2C_SCL);            // set dedicated I2C pins for ESPD-3.5 board

	if (!ts.begin(40))	{ 		  // 40 in this case represents the sensitivity. Try higer or lower for better response.
		Serial.println("Unable to start the capacitive touchscreen.");
	}
  //ts.setRotation(1);		//for older version v2 and before, uses FT6234 touch driver 
  ts.setRotation(3);		// FT5436 touch driver for v2.1 and above

  analogWrite(TFT_BL, TFT_BL_PWM);      // Set brightness of backlight

  displayInit();
  tft.fillScreen(TFT_BLACK);
	tft.setTextSize(1);
	tft.setTextFont(2);
	tft.setTextColor(TFT_WHITE, TFT_BLACK);
	tft.setTextDatum(TL_DATUM);
	tft.drawString("X: ", 3, 0);
	tft.drawString("Y: ", 3, 16);
}

void loop(void)
{

    if (ts.touched())
    {
        // Retrieve a point
        TS_Point p = ts.getPoint();
		tft.setTextColor(TFT_WHITE, TFT_BLACK);
		tft.setTextDatum(TL_DATUM);
		tft.setTextPadding(tft.textWidth("X: 999", 2));
		tft.drawString("X: " + String(p.x), 3, 0);
		tft.setTextPadding(tft.textWidth("Y: 999", 2));
		tft.drawString("Y: " + String(p.y), 3, 16);
    }
    //Debouncing. To avoid returning the same touch multiple times you can play with this delay.
    delay(50);
}