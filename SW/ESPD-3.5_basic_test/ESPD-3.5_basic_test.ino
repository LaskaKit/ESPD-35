/*
ESPD-3.5" - basic test with text and bitmap map in internal memory
Written by laskakit.cz (2022)

Used library:
TFT_eSPI - https://github.com/Bodmer/TFT_eSPI

in User_Setup.h (TFT_eSPI) set ESP32 Dev board pinout to 
// Define driver
#define ILI9488_DRIVER
// The hardware SPI can be mapped to any pins
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15  // Chip select control pin
#define TFT_DC   32  // Data Command control pin
#define TFT_RST  -1
#define TFT_BL   33  // LED back-light
// Backlight
#define TFT_BL   33          
#define TFT_BACKLIGHT_ON HIGH
// SPI freq
#define SPI_FREQUENCY  20000000
*/

#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>
#include "bitmaps.h"
#include "OpenSansSB_40px.h"

TFT_eSPI tft = TFT_eSPI();

void setup()
{
  tft.init();
  tft.setRotation(1);
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
