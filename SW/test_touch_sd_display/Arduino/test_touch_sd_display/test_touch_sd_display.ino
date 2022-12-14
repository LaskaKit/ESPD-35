/* Demo software pro LaskaKit ESPD-3,5" 320x480, ILI9488 
 * Email:podpora@laskakit.cz
 * Web:laskarduino.cz
 * 
 * in User_Setup.h set ESP32 Dev board pinout to 
 * TFT_MISO 12
 * TFT_MOSI 13
 * TFT_SCLK 14
 * TFT_CS   15  // Chip select control pin
 * TFT_DC   32  // Data Command control pin
 * TFT_RST  -1  // Reset pin (could connect to Arduino RESET pin)
 * TFT_BL   33  // LED back-light (required for M5Stack)
 * For Arduino IDE #define SPI_FREQUENCY  20000000
*/
 
#include <TFT_eSPI.h>            // Hardware-specific library
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <Arduino.h>
/*
* Chip used in board is FT5436
* Library used: https://github.com/DustinWatts/FT6236
* Just changed CHIPID and VENDID
* Library is included in the project so it does not need to be downloaded
 */
#include "FT6236.h"


// TFT SPI
#define TFT_LED          33      // TFT backlight pin
#define TFT_LED_PWM      100     // dutyCycle 0-255 last minimum was 15
#define TFT_DISPLAY_RESOLUTION_X  480
#define TFT_DISPLAY_RESOLUTION_Y  320
#define CENTRE_X                  TFT_DISPLAY_RESOLUTION_X/2
#define TFT_GREY 0x7BEF
#define RECT_SIZE_X               100
#define RECT_SIZE_Y               70
// Delay between demo pages
#define WAIT 1000                // Delay between screen tests, set to 0 to demo speed, 2000 to see what it does!

#define SD_CS_PIN     4

SPIClass SDSPI(HSPI);            // Set SD SPI to HSPI
FT6236 ts = FT6236();
TFT_eSPI tft = TFT_eSPI();       // Invoke custom library with default width and height 

uint32_t runTime = 0;

void displayInit(){
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
}

void drawFrame() 
{
  runTime = millis();
  tft.fillRect(0, 0, TFT_DISPLAY_RESOLUTION_X, 13, TFT_RED);

  tft.fillRect(0, 305, TFT_DISPLAY_RESOLUTION_X, TFT_DISPLAY_RESOLUTION_Y, TFT_GREY);
  tft.setTextColor(TFT_BLACK,TFT_RED);

  tft.drawCentreString("* TFT_eSPI *", CENTRE_X, 3, 1);
  tft.setTextColor(TFT_YELLOW,TFT_GREY);
  tft.drawCentreString("Adapted by Bodmer", CENTRE_X, 309, 1);

  tft.drawRect(0, 14, 479, 305 - 14, TFT_BLUE);
}

void drawCrosshairs() 
{
  tft.drawLine(239, 15, 239, 304, TFT_BLUE);
  tft.drawLine(1, 159, 478, 159, TFT_BLUE);
  for (int i = 9; i < 470; i += 10) {
    tft.drawLine(i, 157, i, 161, TFT_BLUE);
  }
  for (int i = 19; i < 220; i += 10) {
    tft.drawLine(237, i, 241, i, TFT_BLUE);
  }
}

void drawGonLines() 
{
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

void drawMovSin() 
{
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

void drawRects() 
{
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

void drawRoundRects() 
{
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

void drawCircles() 
{
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

void drawPatternLines() 
{
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

void drawRandCircles() 
{
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

void drawRandRects() 
{
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

void drawRandRoundRects() 
{
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

void drawRandLines() 
{
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

void drawRandPoints() 
{
  for (int i = 0; i < 10000; i++)
  {
    tft.drawPixel(2 + random(476), 16 + random(289), random(0xFFFF));
  }
}

void drawEndScreen() 
{
  tft.fillRect(0, 0, TFT_DISPLAY_RESOLUTION_X, TFT_DISPLAY_RESOLUTION_Y, TFT_BLUE);

  tft.fillRoundRect(160, 70, 319 - 160, 169 - 70, 3,TFT_RED);
  
  tft.setTextColor(TFT_WHITE,TFT_RED);
  tft.drawCentreString("That's it!", CENTRE_X, 93, 2);
  tft.drawCentreString("Restarting in a", CENTRE_X, 119, 2);
  tft.drawCentreString("few seconds...", CENTRE_X, 132, 2);

  tft.setTextColor(TFT_GREEN,TFT_BLUE);
  tft.drawCentreString("Runtime: (msecs)", CENTRE_X, 280, 2);
  tft.setTextDatum(TC_DATUM);
  runTime = millis() - runTime;
  tft.drawNumber(runTime, CENTRE_X, 300, 2);
  tft.setTextDatum(TL_DATUM);
}

void drawTest() 
{
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
  tft.fillScreen(TFT_BLACK);
}

void touchScreen()
{
  tft.fillScreen(TFT_BLACK);
  tft.fillRoundRect(TFT_DISPLAY_RESOLUTION_X - RECT_SIZE_X, TFT_DISPLAY_RESOLUTION_Y - RECT_SIZE_Y, RECT_SIZE_X, RECT_SIZE_Y, 3, TFT_RED);
  tft.fillRoundRect(0, TFT_DISPLAY_RESOLUTION_Y - RECT_SIZE_Y, RECT_SIZE_X, RECT_SIZE_Y, 3, TFT_RED);
  tft.setTextSize(1);
  tft.setTextFont(4);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.setTextDatum(MC_DATUM); 
  tft.drawString("SD test", (RECT_SIZE_X/2), TFT_DISPLAY_RESOLUTION_Y - (RECT_SIZE_Y / 2));
  tft.setTextDatum(BC_DATUM); 
  tft.drawString("Screen", TFT_DISPLAY_RESOLUTION_X - (RECT_SIZE_X/2), TFT_DISPLAY_RESOLUTION_Y - (RECT_SIZE_Y / 2));
  tft.setTextDatum(TC_DATUM);
  tft.drawString("test", TFT_DISPLAY_RESOLUTION_X - (RECT_SIZE_X/2), TFT_DISPLAY_RESOLUTION_Y - (RECT_SIZE_Y / 2));
  tft.setTextSize(1);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM); 
  tft.drawString("X: ", 3, 0);
  tft.drawString("Y: ", 3, 16);
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.path(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("Message appended");
    } else {
        Serial.println("Append failed");
    }
    file.close();
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}

void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.print("Read from file: ");
    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
}

int SDtest()
{
    if(!SD.begin(SD_CS_PIN, SDSPI)){
        Serial.println("Card Mount Failed");
        return 1;
    }
    uint8_t cardType = SD.cardType();

    if(cardType == CARD_NONE){
        Serial.println("No SD card attached");
        return 1;
    }
    Serial.print("SD Card Type: ");
    if(cardType == CARD_MMC){
        Serial.println("MMC");
    } else if(cardType == CARD_SD){
        Serial.println("SDSC");
    } else if(cardType == CARD_SDHC){
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);
    return 0;
}

void setup() {
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);      // Setting CS pin high
  // configure backlight LED PWM functionalitites
  ledcSetup(1, 5000, 8);              // ledChannel, freq, resolution
  ledcAttachPin(TFT_LED, 1);          // ledPin, ledChannel
  ledcWrite(1, TFT_LED_PWM);          // dutyCycle 0-255
  randomSeed(analogRead(0));
  Serial.begin(115200);
  while (!Serial) {}                  // wait for serial port to connect. Needed for native USB port only
  if (!ts.begin(40))                  //40 in this case represents the sensitivity. Try higer or lower for better response. 
  {
    Serial.println("Unable to start the capacitive touchscreen.");
  }
  displayInit();
  touchScreen();
}

void loop() {
  if (ts.touched())
  {
    TS_Point p = ts.getPoint();
    // Retrieve a point
    // Print coordinates to the screen
    tft.setTextSize(1);
    tft.setTextFont(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TL_DATUM); 
    tft.setTextPadding(tft.textWidth("X: 999", 2));
    tft.drawString("X: " + String(p.x), 3, 0);
    tft.setTextPadding(tft.textWidth("Y: 999", 2));
    tft.drawString("Y: " + String(p.y), 3, 16);
    // Print coordinates to the serial output
    Serial.print("X Coordinate: ");
    Serial.println(p.x);
    Serial.print("Y Coordinate: ");
    Serial.println(p.y);
    // If Screen test touched
    if ((p.x > (320 - RECT_SIZE_Y)) && (p.x < 320) && (p.y < 100) && (p.y > 0))
    {
      drawTest();
      touchScreen();
    }
    // If SD test touched
    else if ((p.x > (320 - RECT_SIZE_Y)) && (p.x < 320) && (p.y < 480) && (p.y > (480 - RECT_SIZE_X)))
    {
      tft.fillScreen(TFT_BLACK);
      tft.setTextSize(1);
      tft.setTextFont(4);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setTextDatum(MC_DATUM);
      if (SDtest() == 1)
      {
        tft.drawString("SD card not found", TFT_DISPLAY_RESOLUTION_X / 2, TFT_DISPLAY_RESOLUTION_Y / 2);
        SD.end();
      }
      else
      {
        tft.setTextDatum(BC_DATUM);
        tft.drawString("SD card mounted", TFT_DISPLAY_RESOLUTION_X / 2, TFT_DISPLAY_RESOLUTION_Y / 2);
        File file = SD.open("/test.txt", FILE_APPEND);
        if(!file)
        {
          writeFile(SD, "/test.txt", "test\n");
        }
        else
        {
          file.close();
          appendFile(SD, "/test.txt", "test\n");
        }
        tft.setTextDatum(TC_DATUM);
        readFile(SD, "/test.txt");
        tft.drawString("text written", TFT_DISPLAY_RESOLUTION_X / 2, TFT_DISPLAY_RESOLUTION_Y / 2);
        SD.end();
      }
      delay(5000);
      touchScreen();
    }
  }
  //Debouncing. To avoid returning the same touch multiple times you can play with this delay.
  delay(50);
}