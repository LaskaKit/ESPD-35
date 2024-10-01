/*
 * ESPD-3.5" - basic test with text and bitmap map in internal memory
 * Written by laskakit.cz (2022)
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

#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>
#include "bitmaps.h"
#include "OpenSansSB_40px.h"

#define TFT_LED          33      // TFT backlight pin
#define TFT_LED_PWM      100     // dutyCycle 0-255 last minimum was 15

TFT_eSPI tft = TFT_eSPI();

void setup()
{
  tft.init();
  tft.setRotation(1);
  
  ledcAttach(TFT_LED, 1000, 8);
  ledcWrite(1, TFT_LED_PWM);
}
void loop()
{ 
    tft.fillScreen(TFT_WHITE);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
   
    /*
    * LCD image convertor 
    * Block size: 16 bit
    * Byte order: Big Endian
    * Rest: default settings
    */
    tft.pushImage(40, 100, 400, 102, laskakit);
    
    tft.setCursor(100,50);
    tft.setFreeFont(&OpenSansSB_40px); 
    tft.print("Bastliri bastli :-) ");

    delay(10000);
}
