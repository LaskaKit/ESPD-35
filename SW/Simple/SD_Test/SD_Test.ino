/* 
 * SD card test for LaskaKit ESPD-3.5" 320x480, ILI9488 https://www.laskakit.cz/laskakit-espd-35-esp32-3-5-tft-ili9488-touch/
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

#include <TFT_eSPI.h> // Hardware-specific library
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <Arduino.h>

// TFT SPI
#define TFT_BL_PWM 255 // Backlight brightness 0-255
#define TFT_DISPLAY_RESOLUTION_X 480
#define TFT_DISPLAY_RESOLUTION_Y 320
#define TFT_GREY 0x7BEF
#define SDTEST_TEXT_PADDING 30

TFT_eSPI tft = TFT_eSPI(); // Invoke custom library with default width and height
SPIClass sdSPI(FSPI);

void displayInit()
{
	tft.init();
	tft.setRotation(1);
	tft.fillScreen(TFT_BLACK);
}

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
	sdSPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_SD_CS);

	if (!SD.begin(SPI_SD_CS, sdSPI))
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
	if (SDtestInit(TFT_DISPLAY_RESOLUTION_X / 2, SDTEST_TEXT_PADDING))
	{
		tft.drawString("SD card not found", TFT_DISPLAY_RESOLUTION_X / 2, SDTEST_TEXT_PADDING);
		SD.end();
		return;
	}
	tft.drawString("SD card mounted", TFT_DISPLAY_RESOLUTION_X / 2, SDTEST_TEXT_PADDING * 2);
	File file = SD.open("/test.txt", FILE_APPEND);
	if (!file)
	{
		if (writeFile(SD, "/test.txt", "test\n"))
		{
			tft.drawString("Unable to write into file", TFT_DISPLAY_RESOLUTION_X / 2, SDTEST_TEXT_PADDING * 3);
			return;
		}
		tft.drawString("File test.txt created, test line written", TFT_DISPLAY_RESOLUTION_X / 2, SDTEST_TEXT_PADDING * 3);
		tft.drawString("Number of lines in the document: " + String(readFile(SD, "/test.txt")), TFT_DISPLAY_RESOLUTION_X / 2, SDTEST_TEXT_PADDING * 4);
		printLastLine(TFT_DISPLAY_RESOLUTION_X / 2, SDTEST_TEXT_PADDING * 5, SD, "/test.txt");
	}
	else
	{
		tft.drawString("Number of lines in the document: " + String(readFile(SD, "/test.txt")), TFT_DISPLAY_RESOLUTION_X / 2, SDTEST_TEXT_PADDING * 3);
		if (appendFile(SD, "/test.txt", "test\n"))
		{
			tft.drawString("Unable to append into test.txt", TFT_DISPLAY_RESOLUTION_X / 2, SDTEST_TEXT_PADDING * 4);
			return;
		}
		tft.drawString("Test line appended to test.txt", TFT_DISPLAY_RESOLUTION_X / 2, SDTEST_TEXT_PADDING * 4);
		tft.drawString("Number of lines in the document: " + String(readFile(SD, "/test.txt")), TFT_DISPLAY_RESOLUTION_X / 2, SDTEST_TEXT_PADDING * 5);
		printLastLine(TFT_DISPLAY_RESOLUTION_X / 2, SDTEST_TEXT_PADDING * 6, SD, "/test.txt");
	}
	SD.end();
}

void setup() {
	displayInit();
	analogWrite(TFT_BL, TFT_BL_PWM);      // Set brightness of backlight
	SDtest();
}

void loop() {}

