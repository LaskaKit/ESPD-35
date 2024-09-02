/* 
 * ADC test for LaskaKit ESPD-3.5" 320x480, ILI9488
 *
 * How to steps:
 * 1. Copy file Setup300_ILI9488_ESPD-3_5.h from https://github.com/LaskaKit/ESPD-35/tree/main/SW to Arduino/libraries/TFT_eSPI/User_Setups/
 * 2. in Arduino/libraries/TFT_eSPI/User_Setup_Select.h
      a. comment: #include <User_Setup.h>
      b. add: #include <User_Setups/Setup300_ILI9488_ESPD-3_5.h>  // Setup file for LaskaKit ESPD-3.5" 320x480, ILI9488
 *
 * Email:podpora@laskakit.cz
 * Web:laskakit.cz
*/

#include <Arduino.h>
#include <TFT_eSPI.h>               // Hardware-specific library
#include <SPI.h>

#define ADC 34                      // Battery voltage mesurement
#define deviderRatio 1.7693877551  // Voltage devider ratio on ADC pin 1MOhm + 1.3MOhm

// TFT SPI
#define TFT_LED 33			// TFT backlight pin
#define TFT_LED_PWM 100 	// dutyCycle 0-255 last minimum was 15
#define TFT_DISPLAY_RESOLUTION_X 480
#define TFT_DISPLAY_RESOLUTION_Y 320

TFT_eSPI tft = TFT_eSPI(); // Invoke custom library with default width and height

void displayInit()
{
	tft.init();
	tft.setRotation(1);
	tft.fillScreen(TFT_BLACK);
}

void setup() {
  ledcAttach(DISPLAY_LED, 5000, 8);
	ledcWrite(1, TFT_LED_PWM);   // dutyCycle 0-255
  displayInit();
}

void loop() {
	tft.setTextSize(1);
	tft.setTextFont(4);
	tft.setTextColor(TFT_WHITE, TFT_BLACK);
	tft.setTextDatum(MC_DATUM);
  tft.drawString("Battery voltage is " + String(analogReadMilliVolts(ADC) * deviderRatio / 1000) + " V", TFT_DISPLAY_RESOLUTION_X / 2, TFT_DISPLAY_RESOLUTION_Y / 2);
  delay(100);
}