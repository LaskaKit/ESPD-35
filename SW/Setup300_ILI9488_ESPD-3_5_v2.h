// Konfigurace pro LaskaKit ESPD-3,5"
// Nakopirovat do slozky Arduino\libraries\TFT_eSPI\User_Setups
// Do souboru Arduino\libraries\TFT_eSPI\User_Setup_Select.h pridat radek: #include <User_Setups/Setup303_ILI9488_ESPD-3_5_v2.h> do sekce #ifndef USER_SETUP_LOADED

#define USER_SETUP_ID 300
#define USER_SETUP_INFO "LaskaKit ESPD-3.5 v2"

// Display:
#define ILI9488_DRIVER
#define TFT_BACKLIGHT_ON HIGH   // Level to turn ON back-light (HIGH or LOW)
#define TFT_BL      33          // LED back-light control pin
#define TFT_MISO    12          // MISO pin
#define TFT_MOSI    13          // MOSI pin
#define TFT_SCLK    14          // Clock pin
#define TFT_CS      15          // Chip select control pin
#define TFT_DC      32          // Data Command control pin
#define TFT_RST     -1          // RST is connectred to EN pin ESP32

// Other pins and constants:
#define POWER_OFF_PIN   17      // Pull LOW to switch board off
#define TOUCH_INT       25      // Touch interrupt pin
// I2C (µŠup and devices):
#define I2C_SDA         21      // Data pin 
#define I2C_SCL         22      // Clock pin
// SPI (SD card):
#define SPI_MISO        19      // MISO pin
#define SPI_MOSI        23      // MOSI pin
#define SPI_SCK         18      // Clock pin
#define SPI_SD_CS       4       // SD Card Chip Select pin

// Battery mesurement:
#define BAT_PIN         34      // Battery voltage mesurement
#define deviderRatio 1.7693877551   // Voltage devider ratio on ADC pin 1MOhm + 1.3MOhm


#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:.
#define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts
#define SMOOTH_FONT

#define SPI_FREQUENCY  75000000 //
//#define SPI_FREQUENCY  79000000 // 79MHz is max. 80MHz has artefacts
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000
#define USE_HSPI_PORT