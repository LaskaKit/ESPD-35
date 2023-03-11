/* Display only test pro LaskaKit ESPD-3.5" 320x480, ILI9488 
 * examle from TFT_eSPI library is used
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

// Delay between demo pages
#define WAIT 1000                // Delay between tests, set to 0 to demo speed, 2000 to see what it does!

#define TFT_DISPLAY_RESOLUTION_X  320
#define TFT_DISPLAY_RESOLUTION_Y  480
#define CENTRE_Y                  TFT_DISPLAY_RESOLUTION_Y/2

#include <TFT_eSPI.h>            // Hardware-specific library
#include <SPI.h>
#include <Arduino.h>

// TFT SPI
#define TFT_LED          33      // TFT backlight pin
#define TFT_LED_PWM      100     // dutyCycle 0-255 last minimum was 15

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library with default width and height

#define TFT_GREY 0x7BEF

uint32_t runTime = 0;

void drawFrame()
{
  tft.fillRect(0, 0, TFT_DISPLAY_RESOLUTION_Y, 13, TFT_RED);

  tft.fillRect(0, 305, TFT_DISPLAY_RESOLUTION_Y, TFT_DISPLAY_RESOLUTION_X, TFT_GREY);
  tft.setTextColor(TFT_BLACK,TFT_RED);

  tft.drawCentreString("* TFT_eSPI *", CENTRE_Y, 3, 1);
  tft.setTextColor(TFT_YELLOW,TFT_GREY);
  tft.drawCentreString("Adapted by Bodmer", CENTRE_Y, 309, 1);

  tft.drawRect(0, 14, 479, 305 - 14, TFT_BLUE);
}

void drawCrosshairs() {
  tft.drawLine(239, 15, 239, 304, TFT_BLUE);
  tft.drawLine(1, 159, 478, 159, TFT_BLUE);
  for (int i = 9; i < 470; i += 10) {
    tft.drawLine(i, 157, i, 161, TFT_BLUE);
  }
  for (int i = 19; i < 220; i += 10) {
    tft.drawLine(237, i, 241, i, TFT_BLUE);
  }
}

void drawGonLines() {
  tft.setTextColor(TFT_CYAN);
  tft.drawString("Sin", 5, 15, 2);
  for (int i = 1; i < 478; i++)
  {
    tft.drawPixel(i, 159 + (sin(((i * 1.13) * 3.14) / 180) * 95), TFT_CYAN);
  }
  
  tft.setTextColor(TFT_RED);
  tft.drawString("Cos", 5, 30, 2);
  for (int i = 1; i < 478; i++)
  {
    tft.drawPixel(i, 159 + (cos(((i * 1.13) * 3.14) / 180) * 95),TFT_RED);
  }

  tft.setTextColor(TFT_YELLOW);
  tft.drawString("Tan", 5, 45, 2);
  for (int i = 1; i < 478; i++)
  {
    tft.drawPixel(i, 159 + (tan(((i * 1.13) * 3.14)/180)),TFT_YELLOW);
  }
}

void drawMovSin() {
  int col = 0;
  int x = 1;
  int y = 0;
  int buf[478];
  for (int i = 1; i < (477 * 15); i++) 
  {
    x++;
    if (x == 478)
      x=1;
    if (i > 478)
    {
      if ((x == 239) || (buf[x-1] == 159))
        col = TFT_BLUE;
      else
        tft.drawPixel(x, buf[x-1], TFT_BLACK);
    }
    y = 159 + (sin(((i * 0.7) * 3.14) / 180) * (90 - (i / 100)));
    tft.drawPixel(x, y, TFT_BLUE);
    buf[x-1] = y;
  }
}

void drawRects() {
  int col = 0;
  for (int i = 1; i < 6; i++)
  {
    switch (i)
    {
      case 1:
        col = TFT_MAGENTA;
        break;
      case 2:
        col = TFT_RED;
        break;
      case 3:
        col = TFT_GREEN;
        break;
      case 4:
        col = TFT_BLUE;
        break;
      case 5:
        col = TFT_YELLOW;
        break;
    }
    tft.fillRect(150 + (i * 20), 70 + (i * 20), 60, 60, col);
  }
}

void drawRoundRects() {
  int col = 0;
  for (int i = 1; i < 6; i++)
  {
    switch (i)
    {
      case 1:
        col = TFT_MAGENTA;
        break;
      case 2:
        col = TFT_RED;
        break;
      case 3:
        col = TFT_GREEN;
        break;
      case 4:
        col = TFT_BLUE;
        break;
      case 5:
        col = TFT_YELLOW;
        break;
    }
    tft.fillRoundRect(270 - (i * 20), 70 + (i * 20), 60, 60, 3, col);
  }
}

void drawCircles() {
  int col = 0;
  for (int i = 1; i < 6; i++)
  {
    switch (i)
    {
      case 1:
        col = TFT_MAGENTA;
        break;
      case 2:
        col = TFT_RED;
        break;
      case 3:
        col = TFT_GREEN;
        break;
      case 4:
        col = TFT_BLUE;
        break;
      case 5:
        col = TFT_YELLOW;
        break;
    }
    tft.fillCircle(180 + (i * 20), 100 + (i * 20), 30, col);
  }
}

void drawPatternLines() {
  for (int i = 15; i < 304; i += 5)
  {
    tft.drawLine(1, i, (i * 1.6) - 10, 303, TFT_RED);
  }

  for (int i = 304; i > 15; i -= 5)
  {
    tft.drawLine(477, i, (i * 1.6) - 11, 15, TFT_RED);
  }

  for (int i = 304; i > 15; i -= 5)
  {
    tft.drawLine(1, i, 491 - (i * 1.6), 15, TFT_CYAN);
  }

  for (int i = 15; i < 304; i += 5)
  {
    tft.drawLine(477, i, 490 - (i * 1.6), 303, TFT_CYAN);
  }
}

void drawRandCircles() {
  int x = 0;
  int y = 0;
  int r = 0;
  for (int i = 0; i < 100; i++)
  {
    x = 32 + random(416);
    y = 45 + random(226);
    r = random(30);
    tft.drawCircle(x, y, r, random(0xFFFF));
  }
}

void drawRandRects() {
  int x = 0;
  int x2 = 0;
  int y = 0;
  int y2 = 0;
  int r = 0;
  for (int i = 0; i < 100; i++)
  {
    x = 2 + random(476);
    y = 16 + random(289);
    x2 = 2 + random(476);
    y2=16 + random(289);
    if (x2 < x) {
      r = x;
      x = x2;
      x2 = r;
    }
    if (y2 < y) {
      r = y;
      y = y2;
      y2 = r;
    }
    tft.drawRect(x, y, x2 - x, y2 - y, random(0xFFFF));
  }
}

void drawRandRoundRects() {
  int x = 0;
  int x2 = 0;
  int y = 0;
  int y2 = 0;
  int r = 0;
  for (int i = 0; i < 100; i++)
  {
    x = 2 + random(476);
    y = 16 + random(289);
    x2 = 2 + random(476);
    y2 = 16 + random(289);
    if (x2 < x) {
      r = x;
      x = x2;
      x2 = r;
    }
    if (y2 < y) {
      r = y;
      y = y2;
      y2 = r;
    }
    tft.drawRoundRect(x, y, x2 - x, y2 - y, 3, random(0xFFFF));
  }
}

void drawRandLines() {
  int x = 0;
  int x2 = 0;
  int y = 0;
  int y2 = 0;
  int col = 0;
  for (int i = 0; i < 100; i++)
  {
    x = 2 + random(476);
    y = 16 + random(289);
    x2 = 2 + random(476);
    y2 = 16 + random(289);
    col = random(0xFFFF);
    tft.drawLine(x, y, x2, y2, col);
  }
}

void drawRandPoints() {
  for (int i = 0; i < 10000; i++)
  {
    tft.drawPixel(2 + random(476), 16 + random(289), random(0xFFFF));
  }
}

void drawEndScreen() {
  tft.fillRect(0, 0, TFT_DISPLAY_RESOLUTION_Y, TFT_DISPLAY_RESOLUTION_X, TFT_BLUE);

  tft.fillRoundRect(160, 70, 319 - 160, 169 - 70, 3,TFT_RED);
  
  tft.setTextColor(TFT_WHITE,TFT_RED);
  tft.drawCentreString("That's it!", CENTRE_Y, 93, 2);
  tft.drawCentreString("Restarting in a", CENTRE_Y, 119, 2);
  tft.drawCentreString("few seconds...", CENTRE_Y, 132, 2);

  tft.setTextColor(TFT_GREEN,TFT_BLUE);
  tft.drawCentreString("Runtime: (msecs)", CENTRE_Y, 280, 2);
  tft.setTextDatum(TC_DATUM);
  runTime = millis() - runTime;
  tft.drawNumber(runTime, CENTRE_Y, 300, 2);
  tft.setTextDatum(TL_DATUM);
}

void setup() {
// configure backlight LED PWM functionalitites
  ledcSetup(1, 5000, 8);              // ledChannel, freq, resolution
  ledcAttachPin(TFT_LED, 1);          // ledPin, ledChannel
  ledcWrite(1, TFT_LED_PWM);          // dutyCycle 0-255

  randomSeed(analogRead(0));
  Serial.begin(115200);
// Setup the LCD
  tft.init();
  tft.setRotation(1);
}

void loop() {
  runTime = millis();
  tft.fillScreen(TFT_BLACK);
  drawFrame();
  drawCrosshairs();
  drawGonLines();
  delay(WAIT);

  tft.fillRect(1, 15, 478 - 1, 304 - 15, TFT_BLACK);
  tft.drawLine(239, 15, 239, 304, TFT_BLUE);
  tft.drawLine(1, 159, 478, 159, TFT_BLUE);
  drawMovSin();
  delay(WAIT);
  tft.fillRect(1, 15, 478 - 1, 304 - 15,TFT_BLACK);
  drawRects();
  delay(WAIT);
  tft.fillRect(1, 15, 478 - 1, 304 - 15, TFT_BLACK);
  drawRoundRects();
  delay(WAIT);
  tft.fillRect(1, 15, 478 - 1, 304 - 15, TFT_BLACK);
  drawCircles();
  delay(WAIT);
  tft.fillRect(1, 15, 478 - 1, 304 - 15, TFT_BLACK);
  drawPatternLines();
  delay(WAIT);
  tft.fillRect(1, 15, 478 - 1, 304 - 15, TFT_BLACK);
  drawRandCircles();
  delay(WAIT);
  tft.fillRect(1, 15, 478 - 1, 304 - 15, TFT_BLACK);
  drawRandRects();
  delay(WAIT);
  tft.fillRect(1, 15, 478 - 1, 304 - 15, TFT_BLACK);
  drawRandRoundRects();
  delay(WAIT);
  tft.fillRect(1 , 15 , 478 - 1, 304 - 15, TFT_BLACK);
  drawRandLines();
  delay(WAIT);
  tft.fillRect(1, 15, 478 - 1, 304 - 15, TFT_BLACK);
  drawRandPoints();
  delay(WAIT);
  tft.fillRect(1, 15, 478 - 1, 304 - 15, TFT_BLUE);
  drawEndScreen();
  delay(10000);
}