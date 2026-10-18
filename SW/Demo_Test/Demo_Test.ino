/*
 * Hardware demo & test for LaskaKit ESPD-3.5" 320x480, ILI9488 https://www.laskakit.cz/laskakit-espd-35-esp32-3-5-tft-ili9488-touch/
 *
 * Home screen with tiles:
 *   Screen   - solid colors, gradients and the TFT_eSPI graphics benchmark (adapted by Bodmer)
 *   SD card  - card type and size, write/read speed, append to /test.txt
 *   I2C scan - lists devices on the µŠup bus with names of the known ones
 *   Music    - plays /Pink-Panther.wav from the SD card through I2S (volume, pause)
 *   Gyro     - artificial horizon from the BMI270 accelerometer
 *   Paint    - two finger drawing to test the capacitive touch panel
 * Header shows SHT4x temperature/humidity and battery voltage, footer shows the last touch coordinates.
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
 * 3. Copy Pink-Panther.wav (or any WAV renamed to it) to the root of the SD card for the Music test
 *
 * Libraries:
 *   TFT_eSPI, ESP32-audioI2S (schreibfaul1), SparkFun BMI270, Adafruit SHT4x
 *
 * Arduino Settings:
		* Board: ESP32S3 Dev Module
		* Flash: 16MB
		* Partition Scheme: Huge APP 3MB
		* PSRAM: OPI PSRAM
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
 *
 * Touch:
 * Chip used in board is FT5436, library: https://github.com/DustinWatts/FT6236
 * Just changed CHIPID and VENDID
 * Library is included in the project so it does not need to be downloaded
 *
 * Email:podpora@laskakit.cz
 * Web:laskakit.cz
*/

#include <Arduino.h>
#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>
#include <Wire.h>
#include "FS.h"
#include "SD.h"
#include "Audio.h"			// schreibfaul1/ESP32-audioI2S
#include <SparkFun_BMI270_Arduino_Library.h>
#include "Adafruit_SHT4x.h"
#include "FT6236.h"

#define TFT_BL_PWM 255 // Backlight brightness 0-255
#define SCR_W 480
#define SCR_H 320

#define BMI270_I2C_ADDRESS 0x68
#define SD_FREQUENCY 20000000

#define MUSIC_FILE "/Pink-Panther.wav"
#define LOG_FILE "/test.txt"
#define SPEED_FILE "/espd_speed.bin"
#define SPEED_TEST_KB 512

#define WAIT 1000					// Delay between screen test pages
#define WAIT_FOREVER 0xFFFFFFFF

// Colors - LaskaKit palette (same as ESPD-InternetRadio)
#define COL_BG          0x1082   // #141110 background
#define COL_CARD        0x2103   // #26211F cards, header
#define COL_CARD_HI     0x39A6   // #3A3431 pressed card, progress track
#define COL_TEAL        0x04D5   // #009BAA LaskaKit primary
#define COL_ORANGE      0xFC27   // #FF873C LaskaKit action color
#define COL_AMBER       0xFDA0   // #FAB400
#define COL_DARK        0x2944   // #2C2826 text on orange
#define COL_TEXT        0xFFFF
#define COL_MUTED       0x9C91   // #9A938F
#define COL_OK          0x2E8E   // #2ED173
#define COL_ERR         0xE249   // #E5484D

// Layout
#define HDR_H     40				// header height, 2px colored line below
#define CONTENT_Y 50
#define MARGIN    12
#define TILE_GAP  10
#define TILE_W    ((SCR_W - 2 * MARGIN - 2 * TILE_GAP) / 3)
#define TILE_H    115
#define FOOTER_Y  (SCR_H - 12)
#define ROW_H     30				// result rows in SD test

#define TILE_COUNT 6

Adafruit_SHT4x sht4 = Adafruit_SHT4x();
Audio audio;
BMI270 bmi270;
FT6236 ts = FT6236(SCR_W, SCR_H);	// Create object for Touch library
TFT_eSPI tft = TFT_eSPI();  		// Invoke custom library with default width and height

// Hardware found during start (SD and music are updated by the SD test)
bool touchOk = false;
bool bmiOk = false;
bool shtOk = false;
bool sdOk = false;
bool musicOk = false;

uint8_t musicVolume = 12;			// 0..21
uint32_t lastStatus = 0;

const char *const tileTitle[TILE_COUNT] = {"Screen", "SD card", "I2C scan", "Music", "Gyro", "Paint"};
const char *const tileSub[TILE_COUNT] = {"Colors & benchmark", "Speed & log test", "Find I2C devices", "WAV from SD card", "Artificial horizon", "Multi-touch test"};
const uint16_t tileColor[TILE_COUNT] = {COL_TEAL, COL_AMBER, COL_TEAL, COL_ORANGE, COL_TEAL, COL_AMBER};

// Touch helpers
/*************************************************************************************************/
bool getTouch(int16_t &x, int16_t &y)
{
	if (!ts.touched())
	{
		return false;
	}
	TS_Point p = ts.getPoint();
	x = p.x;
	y = p.y;
	return true;
}

void waitRelease()
{
	uint32_t start = millis();
	while (ts.touched() && (millis() - start < 5000))
	{
		delay(10);
	}
	delay(40);		// debounce
}

// Returns true when the screen was tapped within ms, waits until the finger is lifted
bool waitTap(uint32_t ms)
{
	uint32_t start = millis();
	do
	{
		if (ts.touched())
		{
			waitRelease();
			return true;
		}
		delay(10);
	} while (millis() - start < ms);
	return false;
}

bool inRect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh)
{
	return (x >= rx) && (x < rx + rw) && (y >= ry) && (y < ry + rh);
}

// Back arrow in the top left corner of every page
bool inBack(int16_t x, int16_t y)
{
	return inRect(x, y, 0, 0, 120, HDR_H + 10);
}

// Common drawing
/*************************************************************************************************/
float batteryVoltage()
{
	return analogReadMilliVolts(BAT_PIN) * deviderRatio / 1000.0;
}

void drawChevron(int16_t x, int16_t y, uint16_t color)
{
	tft.drawWideLine(x + 8, y - 8, x, y, 3, color, COL_CARD);
	tft.drawWideLine(x, y, x + 8, y + 8, 3, color, COL_CARD);
}

// Header with title, optional back arrow and hint on the right
void drawHeader(const char *title, bool back, const char *hint)
{
	tft.fillRect(0, 0, SCR_W, HDR_H, COL_CARD);
	tft.fillRect(0, HDR_H, SCR_W * 3 / 4, 2, COL_TEAL);
	tft.fillRect(SCR_W * 3 / 4, HDR_H, SCR_W / 4, 2, COL_ORANGE);

	int16_t x = MARGIN + 2;
	if (back)
	{
		drawChevron(MARGIN + 2, HDR_H / 2, COL_TEXT);
		x = 40;
	}
	tft.setTextPadding(0);
	tft.setTextColor(COL_TEXT, COL_CARD);
	tft.setTextDatum(ML_DATUM);
	tft.drawString(title, x, HDR_H / 2 + 1, 4);
	if (hint)
	{
		tft.setTextColor(COL_MUTED, COL_CARD);
		tft.setTextDatum(MR_DATUM);
		tft.drawString(hint, SCR_W - MARGIN, HDR_H / 2 + 1, 2);
	}
	tft.setTextDatum(TL_DATUM);
}

void pageStart(const char *title, bool back, const char *hint)
{
	tft.fillScreen(COL_BG);
	drawHeader(title, back, hint);
}

// Centered message for pages that can not run (missing card, sensor...)
void drawMessage(const char *line1, const char *line2, uint16_t color)
{
	tft.setTextPadding(0);
	tft.setTextDatum(MC_DATUM);
	tft.setTextColor(color, COL_BG);
	tft.drawString(line1, SCR_W / 2, SCR_H / 2 - 16, 4);
	tft.setTextColor(COL_MUTED, COL_BG);
	tft.drawString(line2, SCR_W / 2, SCR_H / 2 + 18, 2);
	tft.setTextDatum(TL_DATUM);
}

// Font 2 has no degree sign, draw it as a small circle
// Draws "value°unit" right aligned to x, returns the left edge
int16_t drawDegreesRight(float value, const char *unit, int16_t x, int16_t y, uint16_t fg, uint16_t bg)
{
	tft.setTextDatum(MR_DATUM);
	tft.setTextColor(fg, bg);
	x -= tft.drawString(unit, x, y, 2);
	tft.drawCircle(x - 3, y - 4, 2, fg);
	x -= 7;
	x -= tft.drawString(String(value, 1), x, y, 2);
	tft.setTextDatum(TL_DATUM);
	return x;
}

// Home screen
/*************************************************************************************************/
void tilePos(int i, int16_t &x, int16_t &y)
{
	x = MARGIN + (i % 3) * (TILE_W + TILE_GAP);
	y = CONTENT_Y + (i / 3) * (TILE_H + TILE_GAP);
}

int tileAt(int16_t px, int16_t py)
{
	for (int i = 0; i < TILE_COUNT; i++)
	{
		int16_t x, y;
		tilePos(i, x, y);
		if (inRect(px, py, x, y, TILE_W, TILE_H))
		{
			return i;
		}
	}
	return -1;
}

// -1 = no hardware dependency, 0 = missing, 1 = found
int tileStatus(int i)
{
	switch (i)
	{
	case 1: return sdOk;
	case 3: return musicOk;
	case 4: return bmiOk;
	case 5: return touchOk;
	default: return -1;
	}
}

void drawTileIcon(int i, int16_t x, int16_t y, uint16_t c, uint16_t bg)
{
	switch (i)
	{
	case 0:	// Screen
		tft.drawRoundRect(x, y, 46, 30, 3, c);
		tft.drawRoundRect(x + 1, y + 1, 44, 28, 3, c);
		tft.fillRect(x + 6, y + 6, 11, 18, TFT_RED);
		tft.fillRect(x + 17, y + 6, 12, 18, TFT_GREEN);
		tft.fillRect(x + 29, y + 6, 11, 18, TFT_BLUE);
		tft.fillRect(x + 19, y + 31, 8, 4, c);
		tft.fillRect(x + 12, y + 35, 22, 3, c);
		break;
	case 1:	// SD card
		tft.fillRoundRect(x + 8, y, 30, 38, 3, c);
		tft.fillTriangle(x + 28, y - 1, x + 39, y - 1, x + 39, y + 10, bg);
		for (int k = 0; k < 4; k++)
		{
			tft.fillRect(x + 12 + k * 5, y + 4, 3, 8, bg);
		}
		break;
	case 2:	// I2C bus with three devices
		tft.fillRect(x, y + 4, 46, 3, c);
		tft.fillRect(x, y + 32, 46, 3, c);
		for (int k = 0; k < 3; k++)
		{
			tft.fillRect(x + 6 + k * 16, y + 7, 2, 5, c);
			tft.fillRoundRect(x + 1 + k * 16, y + 12, 12, 15, 2, c);
			tft.fillRect(x + 6 + k * 16, y + 27, 2, 5, c);
		}
		break;
	case 3:	// Music note
		tft.fillCircle(x + 10, y + 31, 7, c);
		tft.fillCircle(x + 35, y + 27, 7, c);
		tft.fillRect(x + 15, y + 6, 3, 25, c);
		tft.fillRect(x + 40, y + 2, 3, 25, c);
		for (int k = 0; k < 5; k++)
		{
			tft.drawLine(x + 15, y + 6 + k, x + 42, y + 2 + k, c);
		}
		break;
	case 4:	// Artificial horizon
		tft.fillCircle(x + 23, y + 19, 19, c);
		tft.fillCircle(x + 23, y + 19, 16, bg);
		tft.drawWideLine(x + 9, y + 23, x + 37, y + 15, 3, c, bg);
		tft.fillCircle(x + 23, y + 19, 3, c);
		break;
	case 5:	// Paint stroke
		for (int k = 0; k < 42; k++)
		{
			tft.fillCircle(x + 2 + k, y + 19 + sin(k * 0.22) * 12, 2, c);
		}
		break;
	}
}

void drawTile(int i, bool pressed)
{
	int16_t x, y;
	tilePos(i, x, y);
	uint16_t bg = pressed ? COL_CARD_HI : COL_CARD;
	tft.fillRoundRect(x, y, TILE_W, TILE_H, 10, bg);
	if (pressed)
	{
		tft.drawRoundRect(x, y, TILE_W, TILE_H, 10, COL_ORANGE);
		tft.drawRoundRect(x + 1, y + 1, TILE_W - 2, TILE_H - 2, 9, COL_ORANGE);
	}
	drawTileIcon(i, x + 16, y + 16, tileColor[i], bg);

	int st = tileStatus(i);
	if (st >= 0)
	{
		tft.fillCircle(x + TILE_W - 16, y + 16, 5, st ? COL_OK : COL_ERR);
	}

	tft.setTextPadding(0);
	tft.setTextDatum(TL_DATUM);
	tft.setTextColor(COL_TEXT, bg);
	tft.drawString(tileTitle[i], x + 16, y + 62, 4);
	tft.setTextColor(COL_MUTED, bg);
	tft.drawString(tileSub[i], x + 16, y + 90, 2);
}

// Temperature, humidity and battery in the home header
void updateStatus()
{
	const int16_t cy = HDR_H / 2 + 1;
	tft.fillRect(SCR_W / 2, 0, SCR_W / 2, HDR_H, COL_CARD);

	// Battery icon
	float v = batteryVoltage();
	int16_t bx = SCR_W - MARGIN - 28;
	int16_t by = cy - 7;
	int level = constrain((int)((v - 3.3) / (4.2 - 3.3) * 22), 0, 22);
	uint16_t bc = (v < 3.5) ? COL_ERR : COL_OK;
	tft.drawRoundRect(bx, by, 26, 14, 2, COL_TEXT);
	tft.fillRect(bx + 26, by + 4, 2, 6, COL_TEXT);
	tft.fillRect(bx + 2, by + 2, level, 10, bc);

	tft.setTextPadding(0);
	tft.setTextDatum(MR_DATUM);
	tft.setTextColor(COL_TEXT, COL_CARD);
	int16_t x = bx - 8;
	x -= tft.drawString(String(v, 2) + " V", x, cy, 2) + 16;

	if (shtOk)
	{
		sensors_event_t humidity, temp;
		if (sht4.getEvent(&humidity, &temp))
		{
			tft.setTextColor(COL_TEAL, COL_CARD);
			x -= tft.drawString(String(humidity.relative_humidity, 0) + " %", x, cy, 2) + 12;
			drawDegreesRight(temp.temperature, "C", x, cy, COL_TEAL, COL_CARD);
		}
	}
	tft.setTextDatum(TL_DATUM);
}

void updateTouchInfo(int16_t x, int16_t y)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "Touch  X: %d  Y: %d", x, y);
	tft.setTextDatum(ML_DATUM);
	tft.setTextColor(COL_MUTED, COL_BG);
	tft.setTextPadding(tft.textWidth("Touch  X: 999  Y: 999", 2));
	tft.drawString(buf, MARGIN, FOOTER_Y, 2);
	tft.setTextPadding(0);
	tft.setTextDatum(TL_DATUM);
}

void drawHome()
{
	tft.fillScreen(COL_BG);
	drawHeader("ESPD-3.5 Demo", false, nullptr);
	updateStatus();
	lastStatus = millis();
	for (int i = 0; i < TILE_COUNT; i++)
	{
		drawTile(i, false);
	}

	updateTouchInfo(0, 0);
	char buf[64];
	snprintf(buf, sizeof(buf), "ESP32-S3 rev %d  |  Flash %d MB  |  PSRAM %d MB", (int)ESP.getChipRevision(),
			 (int)(ESP.getFlashChipSize() >> 20), (int)(ESP.getPsramSize() >> 20));
	tft.setTextDatum(MR_DATUM);
	tft.setTextColor(COL_MUTED, COL_BG);
	tft.drawString(buf, SCR_W - MARGIN, FOOTER_Y, 2);
	tft.setTextDatum(TL_DATUM);
}

// Sceen test functions
/*************************************************************************************************/
void drawFrame()
{
	tft.fillRect(0, 0, SCR_W, 13, TFT_RED);

	tft.fillRect(0, 305, SCR_W, SCR_H, TFT_DARKGREY);
	tft.setTextColor(TFT_BLACK, TFT_RED);

	tft.drawCentreString("* TFT_eSPI *", SCR_W / 2, 3, 1);
	tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);
	tft.drawCentreString("Adapted by Bodmer, tap to return", SCR_W / 2, 309, 1);

	tft.drawRect(0, 14, 479, 305 - 14, TFT_BLUE);
}

void drawGonLines()
{
	// Crosshairs
	tft.drawLine(239, 15, 239, 304, TFT_BLUE);
	tft.drawLine(1, 159, 478, 159, TFT_BLUE);
	for (int i = 9; i < 470; i += 10)
	{
		tft.drawLine(i, 157, i, 161, TFT_BLUE);
	}
	for (int i = 19; i < 220; i += 10)
	{
		tft.drawLine(237, i, 241, i, TFT_BLUE);
	}

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
		tft.drawPixel(i, 159 + (cos(((i * 1.13) * 3.14) / 180) * 95), TFT_RED);
	}

	tft.setTextColor(TFT_YELLOW);
	tft.drawString("Tan", 5, 45, 2);
	for (int i = 1; i < 478; i++)
	{
		tft.drawPixel(i, 159 + (tan(((i * 1.13) * 3.14) / 180)), TFT_YELLOW);
	}
}

void drawMovSin()
{
	int x = 1;
	int y = 0;
	int buf[478];
	tft.drawLine(239, 15, 239, 304, TFT_BLUE);
	tft.drawLine(1, 159, 478, 159, TFT_BLUE);
	for (int i = 1; i < (477 * 15); i++)
	{
		x++;
		if (x == 478)
			x = 1;
		// Erase the previous pass, but keep the axes
		if ((i > 478) && (x != 239) && (buf[x - 1] != 159))
			tft.drawPixel(x, buf[x - 1], TFT_BLACK);
		y = 159 + (sin(((i * 0.7) * 3.14) / 180) * (90 - (i / 100)));
		tft.drawPixel(x, y, TFT_BLUE);
		buf[x - 1] = y;
	}
}

const uint16_t rainbow[5] = {TFT_MAGENTA, TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW};

void drawRects()
{
	for (int i = 1; i < 6; i++)
	{
		tft.fillRect(150 + (i * 20), 70 + (i * 20), 60, 60, rainbow[i - 1]);
	}
}

void drawRoundRects()
{
	for (int i = 1; i < 6; i++)
	{
		tft.fillRoundRect(270 - (i * 20), 70 + (i * 20), 60, 60, 3, rainbow[i - 1]);
	}
}

void drawCircles()
{
	for (int i = 1; i < 6; i++)
	{
		tft.fillCircle(180 + (i * 20), 100 + (i * 20), 30, rainbow[i - 1]);
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
	for (int i = 0; i < 100; i++)
	{
		tft.drawCircle(32 + random(416), 45 + random(226), random(30), random(0xFFFF));
	}
}

// Random rectangle inside the frame, x2 > x and y2 > y
void randRect(int &x, int &y, int &w, int &h)
{
	int x1 = 2 + random(476);
	int y1 = 16 + random(289);
	int x2 = 2 + random(476);
	int y2 = 16 + random(289);
	x = min(x1, x2);
	y = min(y1, y2);
	w = abs(x2 - x1);
	h = abs(y2 - y1);
}

void drawRandRects()
{
	int x, y, w, h;
	for (int i = 0; i < 100; i++)
	{
		randRect(x, y, w, h);
		tft.drawRect(x, y, w, h, random(0xFFFF));
	}
}

void drawRandRoundRects()
{
	int x, y, w, h;
	for (int i = 0; i < 100; i++)
	{
		randRect(x, y, w, h);
		tft.drawRoundRect(x, y, w, h, 3, random(0xFFFF));
	}
}

void drawRandLines()
{
	for (int i = 0; i < 100; i++)
	{
		tft.drawLine(2 + random(476), 16 + random(289), 2 + random(476), 16 + random(289), random(0xFFFF));
	}
}

void drawRandPoints()
{
	for (int i = 0; i < 10000; i++)
	{
		tft.drawPixel(2 + random(476), 16 + random(289), random(0xFFFF));
	}
}

typedef void (*DrawFn)();
const DrawFn benchSteps[] = {drawGonLines, drawMovSin, drawRects, drawRoundRects, drawCircles, drawPatternLines,
							 drawRandCircles, drawRandRects, drawRandRoundRects, drawRandLines, drawRandPoints};

// Red, green, blue and grey ramps - shows the color depth of the panel
void drawGradients()
{
	const int16_t band = SCR_H / 4;
	for (int16_t x = 0; x < SCR_W; x++)
	{
		uint8_t v = x * 255 / (SCR_W - 1);
		tft.drawFastVLine(x, 0, band, tft.color565(v, 0, 0));
		tft.drawFastVLine(x, band, band, tft.color565(0, v, 0));
		tft.drawFastVLine(x, 2 * band, band, tft.color565(0, 0, v));
		tft.drawFastVLine(x, 3 * band, band, tft.color565(v, v, v));
	}
}

void drawEndScreen(uint32_t drawTime)
{
	tft.fillScreen(COL_BG);
	tft.fillRoundRect(SCR_W / 2 - 130, 70, 260, 150, 12, COL_CARD);
	tft.setTextPadding(0);
	tft.setTextDatum(MC_DATUM);
	tft.setTextColor(COL_TEAL, COL_CARD);
	tft.drawString("That's it!", SCR_W / 2, 100, 4);
	tft.setTextColor(COL_MUTED, COL_CARD);
	tft.drawString("Drawing time", SCR_W / 2, 140, 2);
	tft.setTextColor(COL_TEXT, COL_CARD);
	tft.drawString(String(drawTime) + " ms", SCR_W / 2, 175, 4);
	tft.setTextColor(COL_MUTED, COL_BG);
	tft.drawString("Tap to return", SCR_W / 2, 260, 2);
	tft.setTextDatum(TL_DATUM);
}

void screenTest()
{
	// Solid colors - look for dead or stuck pixels
	const uint16_t solid[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_BLACK};
	for (uint16_t c : solid)
	{
		tft.fillScreen(c);
		if (waitTap(500))
			return;
	}
	drawGradients();
	if (waitTap(2000))
		return;

	// Graphics benchmark, only the drawing time is measured
	uint32_t drawTime = 0;
	tft.fillScreen(TFT_BLACK);
	drawFrame();
	for (DrawFn step : benchSteps)
	{
		tft.fillRect(1, 15, 478 - 1, 304 - 15, TFT_BLACK);
		uint32_t start = millis();
		step();
		drawTime += millis() - start;
		if (waitTap(WAIT))
			return;
	}
	drawEndScreen(drawTime);
	waitTap(10000);
}
// End of Screen test functions
/*************************************************************************************************/

// SD test functions
/*************************************************************************************************/
// Label on the left, value on the right; long values use the smaller font
void drawRow(int row, const char *label, const String &value, uint16_t color)
{
	int16_t y = CONTENT_Y + 6 + row * ROW_H;
	int16_t vx = 180;
	int16_t avail = SCR_W - MARGIN - vx;
	tft.setTextDatum(ML_DATUM);
	tft.setTextPadding(0);
	tft.setTextColor(COL_MUTED, COL_BG);
	tft.drawString(label, MARGIN + 12, y, 2);
	tft.setTextColor(color, COL_BG);
	tft.setTextPadding(avail);
	tft.drawString(value, vx, y, tft.textWidth(value, 4) <= avail ? 4 : 2);
	tft.setTextPadding(0);
	tft.setTextDatum(TL_DATUM);
}

String formatSpeed(float kbs)
{
	if (kbs < 0)
		return "failed";
	if (kbs >= 1000)
		return String(kbs / 1024, 2) + " MB/s";
	return String(kbs, 0) + " kB/s";
}

static uint8_t sdBuf[4096];

// Returns kB/s or -1
float sdWriteSpeed()
{
	File file = SD.open(SPEED_FILE, FILE_WRITE);
	if (!file)
	{
		return -1;
	}
	for (int i = 0; i < sizeof(sdBuf); i++)
	{
		sdBuf[i] = i;
	}
	uint32_t start = micros();
	for (int i = 0; i < SPEED_TEST_KB / 4; i++)
	{
		if (file.write(sdBuf, sizeof(sdBuf)) != sizeof(sdBuf))
		{
			file.close();
			return -1;
		}
	}
	file.flush();
	file.close();
	return SPEED_TEST_KB * 1e6f / (micros() - start);
}

// Reads the speed test file back, checks the data and removes it. Returns kB/s or -1
float sdReadSpeed()
{
	File file = SD.open(SPEED_FILE);
	if (!file)
	{
		return -1;
	}
	uint32_t total = 0;
	bool valid = true;
	uint32_t start = micros();
	while (file.available())
	{
		int n = file.read(sdBuf, sizeof(sdBuf));
		if (n <= 0)
			break;
		if (sdBuf[n - 1] != (uint8_t)(n - 1))
			valid = false;
		total += n;
	}
	uint32_t time = micros() - start;
	file.close();
	SD.remove(SPEED_FILE);
	if (!valid || total != SPEED_TEST_KB * 1024)
	{
		return -1;
	}
	return total / 1024.0f * 1e6f / time;
}

// Counts lines of the file and copies the last non-empty one. Returns -1 if the file does not exist
int readLog(char *last, size_t len)
{
	File file = SD.open(LOG_FILE);
	if (!file)
	{
		return -1;
	}
	int lines = 0;
	char cur[64];
	size_t n = 0;
	last[0] = 0;
	while (file.available())
	{
		int count = file.read(sdBuf, sizeof(sdBuf));
		if (count <= 0)
			break;
		for (int i = 0; i < count; i++)
		{
			char c = sdBuf[i];
			if (c == '\n')
			{
				if (n)
				{
					cur[n] = 0;
					strlcpy(last, cur, len);
				}
				lines++;
				n = 0;
			}
			else if ((c != '\r') && (n < sizeof(cur) - 1))
			{
				cur[n++] = c;
			}
		}
	}
	if (n)
	{
		cur[n] = 0;
		strlcpy(last, cur, len);
		lines++;
	}
	file.close();
	return lines;
}

bool appendLog(const char *line)
{
	File file = SD.open(LOG_FILE, FILE_APPEND);
	if (!file)
	{
		return false;
	}
	bool ok = file.print(line) == strlen(line);
	file.close();
	return ok;
}

void sdTest()
{
	pageStart("SD card", true, "Tap to return");

	if (!SD.begin(SPI_SD_CS, SPI, SD_FREQUENCY) || (SD.cardType() == CARD_NONE))
	{
		SD.end();
		sdOk = false;
		musicOk = false;
		drawMessage("SD card not found", "Insert a FAT32 formatted microSD card and try again", COL_ERR);
		waitTap(WAIT_FOREVER);
		return;
	}
	sdOk = true;

	int row = 0;
	uint8_t type = SD.cardType();
	drawRow(row++, "Type", type == CARD_MMC ? "MMC" : type == CARD_SD ? "SDSC" : type == CARD_SDHC ? "SDHC / SDXC" : "unknown", COL_TEXT);
	drawRow(row++, "Card size", String(SD.cardSize() / 1e9, 1) + " GB", COL_TEXT);
	drawRow(row++, "Used / total", String(SD.usedBytes() / 1e9, 2) + " / " + String(SD.totalBytes() / 1e9, 1) + " GB", COL_TEXT);

	drawRow(row, "Write speed", "testing...", COL_MUTED);
	float speed = sdWriteSpeed();
	drawRow(row++, "Write speed", formatSpeed(speed), speed < 0 ? COL_ERR : COL_TEAL);
	drawRow(row, "Read speed", "testing...", COL_MUTED);
	speed = (speed < 0) ? -1 : sdReadSpeed();
	drawRow(row++, "Read speed", formatSpeed(speed), speed < 0 ? COL_ERR : COL_TEAL);

	char last[64];
	int lines = readLog(last, sizeof(last));
	char line[64];
	snprintf(line, sizeof(line), "Test #%d, uptime %lu s\n", max(lines, 0) + 1, millis() / 1000);
	if (appendLog(line))
	{
		lines = readLog(last, sizeof(last));
		drawRow(row++, "Log test.txt", String(lines) + " lines", COL_TEXT);
		drawRow(row++, "Last line", last, COL_TEXT);
	}
	else
	{
		drawRow(row++, "Log test.txt", "unable to write", COL_ERR);
		row++;
	}

	musicOk = SD.exists(MUSIC_FILE);
	drawRow(row++, "Music file", musicOk ? "found" : "missing", musicOk ? COL_OK : COL_ERR);

	SD.end();
	waitTap(WAIT_FOREVER);
}
// End of SD test functions
/*************************************************************************************************/

const char *i2cName(uint8_t address)
{
	switch (address)
	{
	case 0x29: return "TSL2561 / VL53L0X";
	case 0x33: return "MLX90641 IR camera";
	case 0x36: return "MAX17048 fuel gauge";
	case 0x38: return "Touch FT5436";
	case 0x39: return "TSL2561 light";
	case 0x3C:
	case 0x3D: return "OLED display";
	case 0x40: return "INA219 / SHT21";
	case 0x44:
	case 0x45: return "SHT4x temp / hum";
	case 0x51: return "RTC PCF8563";
	case 0x53: return "LTR390 UV";
	case 0x57: return "EEPROM";
	case 0x5A: return "MLX90614 IR temp";
	case 0x62: return "SCD41 CO2";
	case 0x68:
	case 0x69: return "BMI270 IMU";
	case 0x6B: return "SEN66 air quality";
	case 0x76:
	case 0x77: return "BME280 / BMP280";
	default: return "";
	}
}

void I2CTest()
{
	pageStart("I2C scan", true, "Scanning...");

	const int perColumn = 9;
	int nDevices = 0;
	for (uint8_t address = 0x08; address < 0x78; address++)
	{
		Wire.beginTransmission(address);
		if (Wire.endTransmission() != 0)
		{
			continue;
		}
		if (nDevices < 2 * perColumn)
		{
			int16_t x = MARGIN + 12 + (nDevices / perColumn) * (SCR_W / 2);
			int16_t y = CONTENT_Y + 12 + (nDevices % perColumn) * 27;
			char hex[8];
			snprintf(hex, sizeof(hex), "0x%02X", address);
			tft.setTextPadding(0);
			tft.setTextDatum(ML_DATUM);
			tft.setTextColor(COL_TEAL, COL_BG);
			tft.drawString(hex, x, y, 4);
			tft.setTextColor(COL_TEXT, COL_BG);
			tft.drawString(i2cName(address), x + 70, y + 1, 2);
		}
		nDevices++;
	}

	String result = nDevices == 1 ? "1 device" : String(nDevices) + " devices";
	drawHeader("I2C scan", true, (result + "  |  tap to return").c_str());
	if (nDevices == 0)
	{
		drawMessage("No I2C devices found", "Check the uSup connector and pull-ups", COL_ERR);
	}
	tft.setTextDatum(TL_DATUM);
	waitTap(WAIT_FOREVER);
}

// Music test
/*************************************************************************************************/
#define MUSIC_BTN_Y 216
#define MUSIC_BTN_W 132
#define MUSIC_BTN_H 72

int16_t musicBtnX(int i)
{
	return MARGIN + i * (MUSIC_BTN_W + (SCR_W - 2 * MARGIN - 3 * MUSIC_BTN_W) / 2);
}

void drawMusicButton(int i, const char *label, bool primary)
{
	uint16_t bg = primary ? COL_ORANGE : COL_CARD;
	int16_t x = musicBtnX(i);
	tft.fillRoundRect(x, MUSIC_BTN_Y, MUSIC_BTN_W, MUSIC_BTN_H, 10, bg);
	tft.setTextPadding(0);
	tft.setTextDatum(MC_DATUM);
	tft.setTextColor(primary ? COL_DARK : COL_TEXT, bg);
	tft.drawString(label, x + MUSIC_BTN_W / 2, MUSIC_BTN_Y + MUSIC_BTN_H / 2 + 1, 4);
	tft.setTextDatum(TL_DATUM);
}

String formatTime(uint32_t s)
{
	char buf[16];
	snprintf(buf, sizeof(buf), "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
	return buf;
}

void drawMusicCard()
{
	tft.fillRoundRect(MARGIN, CONTENT_Y + 4, SCR_W - 2 * MARGIN, 150, 10, COL_CARD);
	drawTileIcon(3, MARGIN + 18, CONTENT_Y + 22, COL_ORANGE, COL_CARD);
	tft.setTextPadding(0);
	tft.setTextColor(COL_TEXT, COL_CARD);
	tft.setTextDatum(TL_DATUM);
	tft.drawString(MUSIC_FILE + 1, MARGIN + 80, CONTENT_Y + 20, 4);
}

void drawMusicState(const char *state, uint32_t pos, uint32_t len)
{
	const int16_t barX = MARGIN + 18;
	const int16_t barW = SCR_W - 2 * MARGIN - 36;
	const int16_t barY = CONTENT_Y + 100;

	tft.setTextDatum(TL_DATUM);
	tft.setTextColor(COL_MUTED, COL_CARD);
	tft.setTextPadding(200);
	tft.drawString(String(state) + "  |  volume " + String(musicVolume) + " / 21", MARGIN + 80, CONTENT_Y + 52, 2);

	int16_t fill = (len > 0) ? min<int32_t>(barW, (int32_t)barW * pos / len) : 0;
	tft.fillRect(barX, barY, fill, 6, COL_ORANGE);
	tft.fillRect(barX + fill, barY, barW - fill, 6, COL_CARD_HI);

	tft.setTextPadding(60);
	tft.drawString(formatTime(pos), barX, barY + 18, 2);
	tft.setTextDatum(TR_DATUM);
	tft.drawString(formatTime(len), barX + barW, barY + 18, 2);
	tft.setTextPadding(0);
	tft.setTextDatum(TL_DATUM);
}

void musicTest()
{
	pageStart("Music", true, "I2S speaker output");

	if (!SD.begin(SPI_SD_CS, SPI, SD_FREQUENCY) || (SD.cardType() == CARD_NONE))
	{
		SD.end();
		sdOk = false;
		musicOk = false;
		drawMessage("SD card not found", "Copy " MUSIC_FILE " to a microSD card", COL_ERR);
		waitTap(WAIT_FOREVER);
		return;
	}
	sdOk = true;
	musicOk = SD.exists(MUSIC_FILE);
	if (!musicOk)
	{
		SD.end();
		drawMessage("Music file missing", "Copy " MUSIC_FILE " to the root of the SD card", COL_ERR);
		waitTap(WAIT_FOREVER);
		return;
	}

	audio.setVolume(musicVolume);
	audio.connecttoFS(SD, MUSIC_FILE);
	bool paused = false;
	const char *playLabel = "Pause";

	drawMusicCard();
	drawMusicButton(0, "Vol -", false);
	drawMusicButton(1, "Pause", true);
	drawMusicButton(2, "Vol +", false);

	bool prevDown = false;
	uint32_t lastTouch = 0;
	uint32_t lastUi = 0;
	while (true)
	{
		audio.loop();

		// Touch is polled only every 30 ms and never blocks, so the audio does not stutter
		if (millis() - lastTouch >= 30)
		{
			lastTouch = millis();
			int16_t x, y;
			bool down = getTouch(x, y);
			if (down && !prevDown)
			{
				if (inBack(x, y))
				{
					break;
				}
				if (inRect(x, y, musicBtnX(0), MUSIC_BTN_Y, MUSIC_BTN_W, MUSIC_BTN_H) && (musicVolume > 0))
				{
					audio.setVolume(--musicVolume);
					lastUi = 0;
				}
				else if (inRect(x, y, musicBtnX(2), MUSIC_BTN_Y, MUSIC_BTN_W, MUSIC_BTN_H) && (musicVolume < 21))
				{
					audio.setVolume(++musicVolume);
					lastUi = 0;
				}
				else if (inRect(x, y, musicBtnX(1), MUSIC_BTN_Y, MUSIC_BTN_W, MUSIC_BTN_H))
				{
					if (paused || audio.isRunning())
					{
						audio.pauseResume();
						paused = !paused;
					}
					else
					{
						audio.connecttoFS(SD, MUSIC_FILE);		// finished, play again
						paused = false;
					}
					lastUi = 0;
				}
			}
			prevDown = down;
		}

		if (millis() - lastUi >= 250)
		{
			lastUi = millis();
			bool running = audio.isRunning();
			const char *label = paused ? "Play" : (running ? "Pause" : "Replay");
			if (label != playLabel)
			{
				playLabel = label;
				drawMusicButton(1, label, true);
			}
			drawMusicState(paused ? "Paused" : (running ? "Playing" : "Finished"),
						   audio.getAudioCurrentTime(), audio.getAudioFileDuration());
		}
		vTaskDelay(1);
	}

	audio.stopSong();
	SD.end();
	waitRelease();
}

// Gyro test - artificial horizon
/*************************************************************************************************/
#define HZ_H        (SCR_H - 40)		// horizon area, bar with values below
#define HZ_CY       (HZ_H / 2)
#define HZ_PX_DEG   3.5					// pixels per degree of pitch
#define COL_SKY     COL_TEAL
#define COL_GROUND  COL_ORANGE

int16_t horizonY[SCR_W];

int16_t horizonAt(int16_t x, float pitch, float slope)
{
	float y = HZ_CY + pitch * HZ_PX_DEG + (x - SCR_W / 2) * slope;
	return constrain((int)y, 0, HZ_H);
}

void drawAircraft()
{
	const int16_t cx = SCR_W / 2;
	tft.fillRect(cx - 81, HZ_CY - 4, 58, 9, COL_DARK);
	tft.fillRect(cx + 23, HZ_CY - 4, 58, 9, COL_DARK);
	tft.fillRect(cx - 80, HZ_CY - 3, 56, 7, TFT_WHITE);
	tft.fillRect(cx + 24, HZ_CY - 3, 56, 7, TFT_WHITE);
	tft.fillRect(cx - 5, HZ_CY - 5, 11, 11, COL_DARK);
	tft.fillRect(cx - 4, HZ_CY - 4, 9, 9, TFT_WHITE);
}

void drawGyroBar()
{
	tft.fillRect(0, HZ_H, SCR_W, SCR_H - HZ_H, COL_CARD);
	tft.fillRoundRect(6, HZ_H + 5, 90, SCR_H - HZ_H - 10, 6, COL_CARD_HI);
	tft.setTextPadding(0);
	tft.setTextDatum(MC_DATUM);
	tft.setTextColor(COL_TEXT, COL_CARD_HI);
	tft.drawString("Back", 51, HZ_H + 20, 2);
	tft.setTextDatum(MR_DATUM);
	tft.setTextColor(COL_MUTED, COL_CARD);
	tft.drawString("Tap horizon to zero", SCR_W - MARGIN, HZ_H + 20, 2);
	tft.setTextDatum(TL_DATUM);
}

void drawGyroValues(float pitch, float roll)
{
	const int16_t y = HZ_H + 20;
	tft.setTextPadding(0);
	tft.setTextDatum(ML_DATUM);
	tft.setTextColor(COL_MUTED, COL_CARD);
	tft.drawString("Pitch", 120, y, 2);
	tft.drawString("Roll", 240, y, 2);
	tft.fillRect(160, HZ_H, 70, SCR_H - HZ_H, COL_CARD);
	tft.fillRect(272, HZ_H, 70, SCR_H - HZ_H, COL_CARD);
	drawDegreesRight(pitch, "", 225, y, COL_TEXT, COL_CARD);
	drawDegreesRight(roll, "", 337, y, COL_TEXT, COL_CARD);
}

// Note the axes remapping - BMI270 is mounted perpendicular to the display
void readTilt(float &pitch, float &roll)
{
	bmi270.getSensorData();
	float az = bmi270.data.accelX;
	float ay = bmi270.data.accelY;
	float ax = -bmi270.data.accelZ;
	roll = atan2(ay, az) * 180.0 / PI;
	pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0 / PI;
}

void gyroTest()
{
	if (!bmiOk)
	{
		pageStart("Gyro", true, "Tap to return");
		drawMessage("BMI270 not found", "The accelerometer did not answer on I2C address 0x68", COL_ERR);
		waitTap(WAIT_FOREVER);
		return;
	}

	float pitch, roll;
	readTilt(pitch, roll);
	float pitchOffset = pitch;
	float rollOffset = roll;
	float fPitch = 0, fRoll = 0;	// filtered values

	// Full draw once, then only the pixels between the old and the new horizon - no flicker
	for (int16_t x = 0; x < SCR_W; x++)
	{
		horizonY[x] = HZ_CY;
		tft.drawFastVLine(x, 0, HZ_CY, COL_SKY);
		tft.drawFastVLine(x, HZ_CY, HZ_H - HZ_CY, COL_GROUND);
	}
	drawGyroBar();

	bool prevDown = true;
	uint32_t lastValues = 0;
	while (true)
	{
		int16_t tx, ty;
		bool down = getTouch(tx, ty);
		if (down && !prevDown)
		{
			if (inRect(tx, ty, 0, HZ_H, 110, SCR_H - HZ_H))
			{
				break;
			}
			if (ty < HZ_H)
			{
				pitchOffset += fPitch;
				rollOffset += fRoll;
				fPitch = fRoll = 0;
			}
		}
		prevDown = down;

		readTilt(pitch, roll);
		fPitch += 0.25 * ((pitch - pitchOffset) - fPitch);
		fRoll += 0.25 * ((roll - rollOffset) - fRoll);

		float c = cos(fRoll * PI / 180.0);
		float slope = sin(fRoll * PI / 180.0) / (fabs(c) < 0.02 ? (c < 0 ? -0.02 : 0.02) : c);
		for (int16_t x = 0; x < SCR_W; x++)
		{
			int16_t y = horizonAt(x, fPitch, slope);
			if (y < horizonY[x])
				tft.drawFastVLine(x, y, horizonY[x] - y, COL_GROUND);
			else if (y > horizonY[x])
				tft.drawFastVLine(x, horizonY[x], y - horizonY[x], COL_SKY);
			horizonY[x] = y;
		}
		drawAircraft();

		if (millis() - lastValues >= 200)
		{
			lastValues = millis();
			drawGyroValues(fPitch, fRoll);
		}
		delay(10);
	}
	waitRelease();
}

// Paint test - draw with one or two fingers
/*************************************************************************************************/
#define PAINT_BAR_H  44
#define PAINT_COLORS 6

const uint16_t paintColor[PAINT_COLORS] = {COL_TEAL, COL_ORANGE, COL_AMBER, COL_OK, COL_ERR, TFT_WHITE};

void drawPaintBar(int selected)
{
	tft.fillRect(0, 0, SCR_W, PAINT_BAR_H, COL_CARD);
	drawChevron(MARGIN + 2, PAINT_BAR_H / 2, COL_TEXT);
	for (int i = 0; i < PAINT_COLORS; i++)
	{
		int16_t cx = 70 + i * 38;
		tft.fillCircle(cx, PAINT_BAR_H / 2, 16, (i == selected) ? TFT_WHITE : COL_CARD);
		tft.fillCircle(cx, PAINT_BAR_H / 2, 13, paintColor[i]);
	}
	tft.fillRoundRect(SCR_W - 82, 6, 74, PAINT_BAR_H - 12, 6, COL_ORANGE);
	tft.setTextPadding(0);
	tft.setTextDatum(MC_DATUM);
	tft.setTextColor(COL_DARK, COL_ORANGE);
	tft.drawString("Clear", SCR_W - 45, PAINT_BAR_H / 2 + 1, 2);
	tft.setTextDatum(TL_DATUM);
}

void drawPaintCoords(int fingers, int16_t x, int16_t y)
{
	char buf[32];
	if (fingers)
		snprintf(buf, sizeof(buf), "%d: %d, %d", fingers, x, y);
	else
		buf[0] = 0;
	tft.setTextDatum(MR_DATUM);
	tft.setTextColor(COL_MUTED, COL_CARD);
	tft.setTextPadding(tft.textWidth("2: 999, 999", 2));
	tft.drawString(buf, SCR_W - 92, PAINT_BAR_H / 2 + 1, 2);
	tft.setTextPadding(0);
	tft.setTextDatum(TL_DATUM);
}

void paintTest()
{
	int selected = 0;
	tft.fillScreen(COL_BG);
	drawPaintBar(selected);
	tft.setTextDatum(MC_DATUM);
	tft.setTextColor(COL_MUTED, COL_BG);
	tft.drawString("Draw with one or two fingers", SCR_W / 2, (SCR_H + PAINT_BAR_H) / 2, 4);
	tft.setTextDatum(TL_DATUM);
	bool hintShown = true;

	int16_t prevX[2], prevY[2];
	bool hasPrev[2] = {false, false};
	bool prevDown = true;
	while (true)
	{
		uint8_t n = ts.touched();
		if (n == 0)
		{
			hasPrev[0] = hasPrev[1] = false;
			if (prevDown)
				drawPaintCoords(0, 0, 0);
			prevDown = false;
			delay(5);
			continue;
		}

		for (uint8_t i = 0; i < n; i++)
		{
			TS_Point p = ts.getPoint(i);
			if (p.y < PAINT_BAR_H + 4)
			{
				hasPrev[i] = false;
				if (prevDown || (i > 0))
					continue;
				// Toolbar - only on the first contact
				if (p.x < 50)
				{
					waitRelease();
					return;
				}
				if (p.x > SCR_W - 90)
				{
					tft.fillRect(0, PAINT_BAR_H, SCR_W, SCR_H - PAINT_BAR_H, COL_BG);
				}
				for (int c = 0; c < PAINT_COLORS; c++)
				{
					if (abs(p.x - (70 + c * 38)) < 19)
					{
						selected = c;
						drawPaintBar(selected);
					}
				}
				continue;
			}

			if (hintShown)
			{
				tft.fillRect(0, PAINT_BAR_H, SCR_W, SCR_H - PAINT_BAR_H, COL_BG);
				hintShown = false;
			}
			// Touch IDs may swap, a long jump starts a new stroke
			if (hasPrev[i] && (abs(p.x - prevX[i]) + abs(p.y - prevY[i]) < 80))
				tft.drawWideLine(prevX[i], prevY[i], p.x, p.y, 6, paintColor[selected], COL_BG);
			else
				tft.fillCircle(p.x, p.y, 3, paintColor[selected]);
			prevX[i] = p.x;
			prevY[i] = p.y;
			hasPrev[i] = true;
			if (i == 0)
				drawPaintCoords(n, p.x, p.y);
		}
		prevDown = true;
		delay(5);
	}
}

void runTile(int i)
{
	switch (i)
	{
	case 0: screenTest(); break;
	case 1: sdTest(); break;
	case 2: I2CTest(); break;
	case 3: musicTest(); break;
	case 4: gyroTest(); break;
	case 5: paintTest(); break;
	}
}

void setup()
{
	// configure backlight LED PWM functionalitites
	pinMode(TFT_BL, OUTPUT);
	analogWrite(TFT_BL, TFT_BL_PWM);
	Serial.begin(115200);
	Wire.begin(I2C_SDA, I2C_SCL);            // set dedicated I2C pins for ESPD-3.5 board

	tft.init();
	tft.setRotation(1);
	tft.fillScreen(COL_BG);

	randomSeed(esp_random()); // random numbers for Screen test

	touchOk = ts.begin(40);	// 40 in this case represents the sensitivity. Try higer or lower for better response.
	if (!touchOk)
	{
		Serial.println("Unable to start the capacitive touchscreen.");
	}
	//ts.setRotation(1);		//for older version v2 and before, uses FT6234 touch driver
	ts.setRotation(3);		// FT5436 touch driver for v2.1 and above

	bmiOk = (bmi270.beginI2C(BMI270_I2C_ADDRESS) == BMI2_OK);
	if (!bmiOk)
	{
		Serial.println("Unable to start BMI270 gyro.");
	}

	shtOk = sht4.begin();
	if (shtOk)
	{
		sht4.setPrecision(SHT4X_HIGH_PRECISION);
		sht4.setHeater(SHT4X_NO_HEATER);
	}
	else
	{
		Serial.println("Unable to find SHT4x.");
	}

	SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
	sdOk = SD.begin(SPI_SD_CS, SPI, SD_FREQUENCY) && (SD.cardType() != CARD_NONE);
	musicOk = sdOk && SD.exists(MUSIC_FILE);
	SD.end();

	audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);

	drawHome();
}

void loop()
{
	if (millis() - lastStatus > 2000)
	{
		updateStatus();
		lastStatus = millis();
	}

	int16_t x, y;
	if (!getTouch(x, y))
	{
		delay(10);
		return;
	}
	updateTouchInfo(x, y);
	int tile = tileAt(x, y);
	if (tile < 0)
	{
		delay(10);
		return;
	}

	// Highlight the tile and run it when the finger is lifted inside it
	drawTile(tile, true);
	uint32_t start = millis();
	while (getTouch(x, y) && (millis() - start < 5000))
	{
		delay(10);
	}
	delay(40);
	if (tileAt(x, y) == tile)
	{
		runTile(tile);
		drawHome();
	}
	else
	{
		drawTile(tile, false);
	}
}
