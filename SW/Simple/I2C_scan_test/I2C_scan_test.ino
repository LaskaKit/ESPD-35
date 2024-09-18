/* 
 * I2C test for LaskaKit ESPD-3.5" 320x480, ILI9488
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
#include "Wire.h"
#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>

// TFT SPI
#define TFT_LED 33      // TFT backlight pin
#define TFT_LED_PWM 100 // dutyCycle 0-255 last minimum was 15
#define TFT_DISPLAY_RESOLUTION_X 480
#define TFT_DISPLAY_RESOLUTION_Y 320
#define TEXT_PADDING 30

TFT_eSPI tft = TFT_eSPI(); // Invoke custom library with default width and height

void displayInit()
{
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
}

void setup()
{
  Serial.begin(115200);
  Wire.begin();
	// configure backlight LED PWM functionalitites
  ledcAttach(TFT_LED, 1000, 8);
  ledcWrite(1, TFT_LED_PWM);
  displayInit();
}

void loop()
{
  byte error, address;
  int nDevices = 0;

  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextFont(4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(BC_DATUM);
  tft.drawString("Scanning for I2C devices...", TFT_DISPLAY_RESOLUTION_X / 2, TEXT_PADDING);
  for (address = 0x01; address < 0x7f; address++)
  {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0)
    {
      nDevices++;
      tft.drawString("I2C device found at address 0x" + String(address), TFT_DISPLAY_RESOLUTION_X / 2, TEXT_PADDING * nDevices + TEXT_PADDING);
    }
    else if (error != 2)
    {
      if (nDevices)
      {
        tft.drawString("Error" + String(error) + "at address 0x" + String(address), TFT_DISPLAY_RESOLUTION_X / 2, TEXT_PADDING * nDevices + 2 * TEXT_PADDING);
      }
      else
      {
        tft.drawString("Error" + String(error) + "at address 0x" + String(address), TFT_DISPLAY_RESOLUTION_X / 2, TEXT_PADDING * 2);
      }
    }
  }
  if (nDevices == 0)
  {
    tft.drawString("No I2C devices found", TFT_DISPLAY_RESOLUTION_X / 2, TEXT_PADDING * 2);
    tft.drawString("Next scanning in 5 seconds...", TFT_DISPLAY_RESOLUTION_X / 2, TEXT_PADDING * 3);
  }
  else
  {
    tft.drawString("Next scanning in 5 seconds...", TFT_DISPLAY_RESOLUTION_X / 2, TEXT_PADDING * nDevices + 2 * TEXT_PADDING);
  }
  delay(5000);
}