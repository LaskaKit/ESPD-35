// Konfigurace pro LaskaKit ESPD-3,5"
// Nakopirovat do slozky Arduino\libraries\TFT_eSPI\User_Setups
// Do souboru Arduino\libraries\TFT_eSPI\User_Setup_Select.h pridat radek: #include <User_Setups/Setup303_ILI9488_ESPD-3_5_v3.h> do sekce #ifndef USER_SETUP_LOADED

#define USER_SETUP_ID 303
#define USER_SETUP_INFO "LaskaKit ESPD-3.5 v3"

// Display:
#define ILI9488_DRIVER
#define TFT_BACKLIGHT_ON HIGH   // Level to turn ON back-light (HIGH or LOW)
#define TFT_BL      45          // LED back-light control pin
#define TFT_MISO    13          // MISO pin
#define TFT_MOSI    11          // MOSI pin
#define TFT_SCLK    12          // Clock pin
#define TFT_CS      48          // Chip select control pin
#define TFT_DC      47          // Data Command control pin
#define TFT_RST     -1          // RST is connectred to EN pin ESP32

// Other pins and constants:
#define POWER_OFF_PIN   16      // Pull LOW to switch board off
#define TOUCH_INT       10      // Touch interrupt pin
// I2C (µŠup and devices):
#define I2C_SDA         42      // Data pin 
#define I2C_SCL         2       // Clock pin
// SPI (µŠup and SD card):
#define SPI_MISO        21      // MISO pin
#define SPI_MOSI        38      // MOSI pin
#define SPI_SCK         14      // Clock pin
#define SPI_USUP_CS     46      // µŠup Chip Select pin
#define SPI_SD_CS       17      // SD Card Chip Select pin
// I2S:
#define I2S_LRC         39      // Word select a.k.a. left-right clock pin
#define I2S_DOUT        40      // Serial data pin
#define I2S_BCLK        41      // Serial clock a.k.a. bit clock pin
// Battery mesurement:
#define BAT_PIN         9       // Battery voltage mesurement
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