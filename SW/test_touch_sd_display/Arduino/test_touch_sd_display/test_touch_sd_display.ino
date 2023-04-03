/* 
 * Display, SD card and touch test for LaskaKit ESPD-3.5" 320x480, ILI9488
 * example from TFT_eSPI library is used
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
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <ESP32AnalogRead.h>
#include <Arduino.h>
/*
 * Chip used in board is FT5436
 * Library used: https://github.com/DustinWatts/FT6236
 * Just changed CHIPID and VENDID
 * Library is included in the project so it does not need to be downloaded
 */
#include "FT6236.h"

// Set your version of display (V2.0 uses FT6234 touch driver and V2.1 uses FT5436)
#define V2_0
//#define V2_1

// TFT SPI
#define TFT_LED 33		// TFT backlight pin
#define TFT_LED_PWM 100 // dutyCycle 0-255 last minimum was 15
#define TFT_RES_X 480
#define TFT_RES_Y 320
#define CENTRE_X TFT_RES_X / 2
#define TFT_GREY 0x7BEF
#define RECT_SIZE_X 100
#define RECT_SIZE_Y 70
#define TEST_TEXT_PADDING 30

// Delay between demo pages
#define WAIT 1000

#define SD_CS_PIN 4
#define POWER_OFF_PIN 17
#define ADC_PIN 34                      // Battery voltage mesurement
#define deviderRatio 1.3

FT6236 ts = FT6236(480, 320);	// Create object for Touch library
TFT_eSPI tft = TFT_eSPI();  	// Invoke custom library with default width and height
ESP32AnalogRead adc;			// Create object for adc library

uint32_t runTime = 0;			// Variable for measuring Screen test time

void displayInit()
{
	tft.init();
	tft.setRotation(1);
	tft.fillScreen(TFT_BLACK);
}

// Delay function with milis()
uint8_t isTouched(uint16_t time)
{
	unsigned long currentMillis = millis();
	unsigned long previousMillis = currentMillis;
	while ((currentMillis - previousMillis) < time)
	{
		if (ts.touched())
		{
			return 1;
		}
		currentMillis = millis();
	}
	return 0;
}

// Sceen test functions
/*************************************************************************************************/
void drawFrame()
{
	runTime = millis();
	tft.fillRect(0, 0, TFT_RES_X, 13, TFT_RED);

	tft.fillRect(0, 305, TFT_RES_X, TFT_RES_Y, TFT_GREY);
	tft.setTextColor(TFT_BLACK, TFT_RED);

	tft.drawCentreString("* TFT_eSPI *", CENTRE_X, 3, 1);
	tft.setTextColor(TFT_YELLOW, TFT_GREY);
	tft.drawCentreString("Adapted by Bodmer", CENTRE_X, 309, 1);

	tft.drawRect(0, 14, 479, 305 - 14, TFT_BLUE);
}

void drawCrosshairs()
{
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
	int col = 0;
	int x = 1;
	int y = 0;
	int buf[478];
	for (int i = 1; i < (477 * 15); i++)
	{
		x++;
		if (x == 478)
			x = 1;
		if (i > 478)
		{
			if ((x == 239) || (buf[x - 1] == 159))
				col = TFT_BLUE;
			else
				tft.drawPixel(x, buf[x - 1], TFT_BLACK);
		}
		y = 159 + (sin(((i * 0.7) * 3.14) / 180) * (90 - (i / 100)));
		tft.drawPixel(x, y, TFT_BLUE);
		buf[x - 1] = y;
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
		y2 = 16 + random(289);
		if (x2 < x)
		{
			r = x;
			x = x2;
			x2 = r;
		}
		if (y2 < y)
		{
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
		if (x2 < x)
		{
			r = x;
			x = x2;
			x2 = r;
		}
		if (y2 < y)
		{
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
	tft.fillRect(0, 0, TFT_RES_X, TFT_RES_Y, TFT_BLUE);

	tft.fillRoundRect(160, 70, 319 - 160, 169 - 70, 3, TFT_RED);

	tft.setTextColor(TFT_WHITE, TFT_RED);
	tft.drawCentreString("That's it!", CENTRE_X, 93, 2);
	tft.drawCentreString("Restarting in a", CENTRE_X, 119, 2);
	tft.drawCentreString("few seconds...", CENTRE_X, 132, 2);

	tft.setTextColor(TFT_GREEN, TFT_BLUE);
	tft.drawCentreString("Runtime: (msecs)", CENTRE_X, 280, 2);
	tft.setTextDatum(TC_DATUM);
	runTime = millis() - runTime;
	tft.drawNumber(runTime, CENTRE_X, 300, 2);
	tft.setTextDatum(TL_DATUM);
}

void screenTest()
{
	tft.fillScreen(TFT_BLACK);
	drawFrame();
	drawCrosshairs();
	drawGonLines();
	if (isTouched(WAIT))
		return;
	tft.fillRect(1, 15, 478 - 1, 304 - 15, TFT_BLACK);
	tft.drawLine(239, 15, 239, 304, TFT_BLUE);
	tft.drawLine(1, 159, 478, 159, TFT_BLUE);
	drawMovSin();
	if (isTouched(WAIT))
		return;
	tft.fillRect(1, 15, 478 - 1, 304 - 15, TFT_BLACK);
	drawRects();
	if (isTouched(WAIT))
		return;
	tft.fillRect(1, 15, 478 - 1, 304 - 15, TFT_BLACK);
	drawRoundRects();
	if (isTouched(WAIT))
		return;
	tft.fillRect(1, 15, 478 - 1, 304 - 15, TFT_BLACK);
	drawCircles();
	if (isTouched(WAIT))
		return;
	tft.fillRect(1, 15, 478 - 1, 304 - 15, TFT_BLACK);
	drawPatternLines();
	if (isTouched(WAIT))
		return;
	tft.fillRect(1, 15, 478 - 1, 304 - 15, TFT_BLACK);
	drawRandCircles();
	if (isTouched(WAIT))
		return;
	tft.fillRect(1, 15, 478 - 1, 304 - 15, TFT_BLACK);
	drawRandRects();
	if (isTouched(WAIT))
		return;
	tft.fillRect(1, 15, 478 - 1, 304 - 15, TFT_BLACK);
	drawRandRoundRects();
	if (isTouched(WAIT))
		return;
	tft.fillRect(1, 15, 478 - 1, 304 - 15, TFT_BLACK);
	drawRandLines();
	if (isTouched(WAIT))
		return;
	tft.fillRect(1, 15, 478 - 1, 304 - 15, TFT_BLACK);
	drawRandPoints();
	if (isTouched(WAIT))
		return;
	tft.fillRect(1, 15, 478 - 1, 304 - 15, TFT_BLUE);
	drawEndScreen();
	if (isTouched(WAIT))
		return;
	tft.fillScreen(TFT_BLACK);
}
// End of Screen test functions
/*************************************************************************************************/

// Print touch coordinates on display
void printCoordinates(TS_Point p)
{
	tft.setTextSize(1);
	tft.setTextFont(2);
	tft.setTextColor(TFT_WHITE, TFT_BLACK);
	tft.setTextDatum(TL_DATUM);
	tft.setTextPadding(tft.textWidth("X: 999", 2));
	tft.drawString("X: " + String(p.x), 3, 0);
	tft.setTextPadding(tft.textWidth("Y: 999", 2));
	tft.drawString("Y: " + String(p.y), 3, 16);
}

// Print battery voltage on display
void printVoltage()
{
	tft.setTextSize(1);
	tft.setTextFont(2);
	tft.setTextColor(TFT_WHITE, TFT_BLACK);
	tft.setTextDatum(TR_DATUM);
	tft.setTextPadding(tft.textWidth("9.99 V", 2));
	tft.drawString(String(adc.readVoltage() * deviderRatio) + " V", TFT_RES_X - 3, 0);
}

// Print initial screen
void touchScreen()
{
	tft.fillScreen(TFT_BLACK);
	tft.setTextSize(1);
	tft.setTextFont(4);
	// Print SD test button
	tft.setTextColor(TFT_WHITE, TFT_BROWN);
	tft.fillRoundRect(0, TFT_RES_Y - RECT_SIZE_Y, RECT_SIZE_X, RECT_SIZE_Y, 3, TFT_BROWN);
	tft.setTextDatum(MC_DATUM);
	tft.drawString("SD test", (RECT_SIZE_X / 2), TFT_RES_Y - (RECT_SIZE_Y / 2));
	// Print I2C scanner button
	tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
	tft.fillRoundRect(RECT_SIZE_X + 27, TFT_RES_Y - RECT_SIZE_Y, RECT_SIZE_X, RECT_SIZE_Y, 3, TFT_DARKGREEN);
	tft.setTextDatum(BC_DATUM);
	tft.drawString("I2C", (RECT_SIZE_X / 2) + RECT_SIZE_X + 27, TFT_RES_Y - (RECT_SIZE_Y / 2));
	tft.setTextDatum(TC_DATUM);
	tft.drawString("scanner", (RECT_SIZE_X / 2) + RECT_SIZE_X + 27, TFT_RES_Y - (RECT_SIZE_Y / 2));
	// Print Power off button
	tft.setTextColor(TFT_WHITE, TFT_RED);
	tft.fillRoundRect(2 * RECT_SIZE_X + 54, TFT_RES_Y - RECT_SIZE_Y, RECT_SIZE_X, RECT_SIZE_Y, 3, TFT_RED);
	tft.setTextDatum(BC_DATUM);
	tft.drawString("Power", (RECT_SIZE_X / 2) + 2 * RECT_SIZE_X + 54, TFT_RES_Y - (RECT_SIZE_Y / 2));
	tft.setTextDatum(TC_DATUM);
	tft.drawString("off", (RECT_SIZE_X / 2) + 2 * RECT_SIZE_X + 54, TFT_RES_Y - (RECT_SIZE_Y / 2));
	// Print Screen test button
	tft.setTextColor(TFT_WHITE, TFT_BLUE);
	tft.fillRoundRect(TFT_RES_X - RECT_SIZE_X, TFT_RES_Y - RECT_SIZE_Y, RECT_SIZE_X, RECT_SIZE_Y, 3, TFT_BLUE);
	tft.setTextDatum(BC_DATUM);
	tft.drawString("Screen", TFT_RES_X - (RECT_SIZE_X / 2), TFT_RES_Y - (RECT_SIZE_Y / 2));
	tft.setTextDatum(TC_DATUM);
	tft.drawString("test", TFT_RES_X - (RECT_SIZE_X / 2), TFT_RES_Y - (RECT_SIZE_Y / 2));

	tft.setTextSize(1);
	tft.setTextFont(2);
	tft.setTextColor(TFT_WHITE, TFT_BLACK);
	tft.setTextDatum(TL_DATUM);
	tft.drawString("X: ", 3, 0);
	tft.drawString("Y: ", 3, 16);
	printVoltage();
}

// SD test functions
/*************************************************************************************************/
int appendFile(fs::FS &fs, const char *path, const char *message)
{
	File file = fs.open(path, FILE_APPEND);
	if (!file)
	{
		return -1;
	}
	if (file.print(message))
	{
		file.close();
	}
	else
	{
		file.close();
		return -2;
	}
	return 0;
}

int writeFile(fs::FS &fs, const char *path, const char *message)
{
	File file = fs.open(path, FILE_WRITE);
	if (!file)
	{
		return -1;
	}
	if (file.print(message))
	{
		file.close();
	}
	else
	{
		file.close();
		return -2;
	}
	return 0;
}

int readFile(fs::FS &fs, const char *path)
{
	uint16_t lines = 0;
	File file = fs.open(path);
	if (!file)
	{
		return -1;
	}
	while (file.available())
	{
		if (file.read() == '\n')
		{
			lines++;
		}
	}
	file.close();
	return lines;
}

// Print last line of file
void printLastLine(int32_t x, int32_t y, fs::FS &fs, const char *path)
{
	int lines = readFile(fs, path);
	int actLines = 1;
	char text[10] = {0};
	File file = fs.open(path);
	if (!file)
	{
		tft.drawString("Unable to open file", x, y);
		return;
	}
	while (actLines < lines)
	{
		char t = file.read();
		if (t == '\n')
		{
			actLines++;
		}
	}
	int i = 0;
	while (file.available())
	{
		char t = file.read();
		text[i++] = t;
	}
	file.close();
	tft.drawString("Last line written: " + String(text), x, y);
}

int SDtestInit(int32_t x, int32_t y)
{
	uint8_t cardType;
	uint64_t cardSize;
	if (!SD.begin(SD_CS_PIN))
	{
		return -1;
	}
	cardType = SD.cardType();
	if (cardType == CARD_NONE)
	{
		return -1;
	}
	cardSize = SD.cardSize() / (1024 * 1024);
	if (cardType == CARD_MMC)
	{
		tft.drawString("SD Card Type: MMC, size: " + String(cardSize) + "Mb", x, y);
	}
	else if (cardType == CARD_SD)
	{
		tft.drawString("SD Card Type: SDSC, size: " + String(cardSize) + "Mb", x, y);
	}
	else if (cardType == CARD_SDHC)
	{
		tft.drawString("SD Card Type: SDHC, size: " + String(cardSize) + "Mb", x, y);
	}
	else
	{
		tft.drawString("SD Card Type: UNKNOWN, size: " + String(cardSize) + "Mb", x, y);
	}
	return 0;
}

void SDtest()
{
	tft.fillScreen(TFT_BLACK);
	tft.setTextSize(1);
	tft.setTextFont(4);
	tft.setTextColor(TFT_WHITE, TFT_BLACK);
	tft.setTextDatum(BC_DATUM);
	if (SDtestInit(TFT_RES_X / 2, TEST_TEXT_PADDING))
	{
		tft.drawString("SD card not found", TFT_RES_X / 2, TEST_TEXT_PADDING);
		tft.drawString("Touch to return to the main page", TFT_RES_X / 2, TEST_TEXT_PADDING * 2);
		SD.end();
		return;
	}
	tft.drawString("SD card mounted", TFT_RES_X / 2, TEST_TEXT_PADDING * 2);
	File file = SD.open("/test.txt", FILE_APPEND);
	if (!file)
	{
		if (writeFile(SD, "/test.txt", "test\n"))
		{
			tft.drawString("Unable to write into file", TFT_RES_X / 2, TEST_TEXT_PADDING * 3);
			tft.drawString("Touch to return to the main page", TFT_RES_X / 2, TEST_TEXT_PADDING * 4);
			return;
		}
		tft.drawString("File test.txt created, test line written", TFT_RES_X / 2, TEST_TEXT_PADDING * 3);
		tft.drawString("Number of lines in the document: " + String(readFile(SD, "/test.txt")), TFT_RES_X / 2, TEST_TEXT_PADDING * 4);
		printLastLine(TFT_RES_X / 2, TEST_TEXT_PADDING * 5, SD, "/test.txt");
		tft.drawString("Touch to return to the main page", TFT_RES_X / 2, TEST_TEXT_PADDING * 6);
	}
	else
	{
		tft.drawString("Number of lines in the document: " + String(readFile(SD, "/test.txt")), TFT_RES_X / 2, TEST_TEXT_PADDING * 3);
		if (appendFile(SD, "/test.txt", "test\n"))
		{
			tft.drawString("Unable to append into test.txt", TFT_RES_X / 2, TEST_TEXT_PADDING * 4);
			tft.drawString("Touch to return to the main page", TFT_RES_X / 2, TEST_TEXT_PADDING * 5);
			return;
		}
		tft.drawString("Test line appended to test.txt", TFT_RES_X / 2, TEST_TEXT_PADDING * 4);
		tft.drawString("Number of lines in the document: " + String(readFile(SD, "/test.txt")), TFT_RES_X / 2, TEST_TEXT_PADDING * 5);
		printLastLine(TFT_RES_X / 2, TEST_TEXT_PADDING * 6, SD, "/test.txt");
		tft.drawString("Touch to return to the main page", TFT_RES_X / 2, TEST_TEXT_PADDING * 7);
	}
	SD.end();
	while (!ts.touched());
}
// End of SD test functions
/*************************************************************************************************/

void I2CTest()
{
	byte error, address;
	int nDevices = 0;

	tft.fillScreen(TFT_BLACK);
	tft.setTextSize(1);
	tft.setTextFont(4);
	tft.setTextColor(TFT_WHITE, TFT_BLACK);
	tft.setTextDatum(BC_DATUM);
	tft.drawString("Scanning for I2C devices...", TFT_RES_X / 2, TEST_TEXT_PADDING);
	for (address = 0x01; address < 0x7f; address++)
	{
		Wire.beginTransmission(address);
		error = Wire.endTransmission();
		if (error == 0)
		{
			nDevices++;
			tft.drawString("I2C device found at address 0x" + String(address), TFT_RES_X / 2, TEST_TEXT_PADDING * nDevices + TEST_TEXT_PADDING);
		}
		else if (error != 2)
		{
			if (nDevices)
			{
				tft.drawString("Error" + String(error) + "at address 0x" + String(address), TFT_RES_X / 2, TEST_TEXT_PADDING * nDevices + 2 * TEST_TEXT_PADDING);
			}
			else
			{
				tft.drawString("Error" + String(error) + "at address 0x" + String(address), TFT_RES_X / 2, TEST_TEXT_PADDING * 2);
			}
		}
	}
	if (nDevices == 0)
	{
		tft.drawString("No I2C devices found", TFT_RES_X / 2, TEST_TEXT_PADDING * 2);
		tft.drawString("Touch to return to the main page", TFT_RES_X / 2, TEST_TEXT_PADDING * 3);
	}
	else
	{
		tft.drawString("Touch to return to the main page", TFT_RES_X / 2, TEST_TEXT_PADDING * nDevices + 2 * TEST_TEXT_PADDING);
	}
	while (!ts.touched());
}

void setup()
{
	// configure backlight LED PWM functionalitites
	ledcSetup(1, 5000, 8);	   // ledChannel, freq, resolution
	ledcAttachPin(TFT_LED, 1); // ledPin, ledChannel
	ledcWrite(1, TFT_LED_PWM); // dutyCycle 0-255

	randomSeed(analogRead(0)); // get random number for Screen test
	adc.attach(ADC_PIN);
	Serial.begin(115200);
	if (!ts.begin(40)) 		  // 40 in this case represents the sensitivity. Try higer or lower for better response.
	{
		Serial.println("Unable to start the capacitive touchscreen.");
	}
	#ifdef V2_0
    	ts.setRotation(1);
  	#endif
  	#ifdef V2_1
    	ts.setRotation(3);
  	#endif
	displayInit();
	touchScreen();
}

void loop()
{
	if (ts.touched())
	{
		TS_Point p = ts.getPoint();
		printCoordinates(p);
		printVoltage();
		// If Screen test touched
		if ((p.x > (TFT_RES_X - RECT_SIZE_X)) && (p.x < TFT_RES_X) && (p.y > (TFT_RES_Y - RECT_SIZE_Y)) && (p.y < TFT_RES_Y))
		{
			screenTest();
			touchScreen();
		}
		// If SD test touched
		else if ((p.x > 0) && (p.x < (RECT_SIZE_X)) && (p.y > (TFT_RES_Y - RECT_SIZE_Y)) && (p.y < TFT_RES_Y))
		{
			SDtest();
			if (isTouched(5000))
				touchScreen();
			else
				touchScreen();
		}
		// If I2C scanner touched
		else if ((p.x > (RECT_SIZE_X + 27)) && (p.x < (2 * RECT_SIZE_X + 27)) && (p.y > (TFT_RES_Y - RECT_SIZE_Y)) && (p.y < TFT_RES_Y))
		{
			I2CTest();
			touchScreen();
		}
		// If Power off touched
		else if ((p.x > (2 * RECT_SIZE_X + 54)) && (p.x < (3 * RECT_SIZE_X + 54)) && (p.y > (TFT_RES_Y - RECT_SIZE_Y)) && (p.y < TFT_RES_Y))
		{
			pinMode(POWER_OFF_PIN, OUTPUT);
			digitalWrite(POWER_OFF_PIN, LOW);
		}
	}
}