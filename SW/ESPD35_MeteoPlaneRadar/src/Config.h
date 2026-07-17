// =============================================================================
//  ESPD35_MeteoPlaneRadar - radar letadel (adsb.fi) na LaskaKit ESPD-3.5"
//  Port projektu petus/Meteo-PlaneRadar (kulaty 480x480) na obdelnikovy
//  displej ILI9488 480x320 (SPI), ESP32-S3.
//
//  >>> TENTO SOUBOR JE JEDINY, KTERY VETSINOU MUSITE UPRAVIT <<<
//  Zkontrolujte piny podle pinoutu vasi desky ESPD-3.5 a ukazek v
//  https://github.com/LaskaKit/ESPD-35/tree/main/SW
// =============================================================================
#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
//  Rozmery displeje (na sirku / landscape). ILI9488 je nativne 320x480,
//  rotaci 1 (nebo 3) ho otocime na 480x320.
// ---------------------------------------------------------------------------
#define LCD_WIDTH    480
#define LCD_HEIGHT   320
#define LCD_ROTATION 1        // 1 nebo 3 (podle orientace desky); 0/2 = na vysku

// ---------------------------------------------------------------------------
//  SPI piny displeje ILI9488.
//  !!! OVERTE podle pinoutu ESPD-3.5 - hodnoty nize jsou orientacni !!!
//  Puvodni ESPD-3.5 (ESP32-WROVER) pouziva: SCK14 MOSI13 MISO12 CS15 DC2 BL27.
//  Na variante s ESP32-S3 se piny lisi - doplnte je z ukazek v adresari SW/.
// ---------------------------------------------------------------------------
#define TFT_SCK    12
#define TFT_MOSI   11
#define TFT_MISO   13         // -1 pokud MISO neni potreba
#define TFT_CS     48
#define TFT_DC     47
#define TFT_RST    -1         // -1 pokud je RST na EN/napajeni
#define TFT_BL     45         // podsviceni (PWM)

// Rychlost SPI sbernice displeje (ILI9488 zvlada 40-80 MHz).
#define TFT_SPI_HZ 40000000

// ---------------------------------------------------------------------------
//  Ovladaci tlacitko.
//  ESPD-3.5 ma tlacitko na GPIO35 (verze WROVER). POZOR: na ESP32-S3 s OPI
//  PSRAM jsou GPIO35/36/37 obsazene PSRAM - pak pouzijte jiny pin (napr. BOOT
//  na GPIO0). Nastavte podle sve desky.
//    kratky stisk  = zmena rozsahu (10/25/50/100 km)
//    dlouhy stisk  = prepnuti jednotek (letecke <-> metricke)
//    drzeni pri startu ~3 s = reset WiFi/nastaveni
// ---------------------------------------------------------------------------
#define BUTTON_PIN        0    // 0 = BOOT tlacitko; zmente dle desky
#define BUTTON_ACTIVE_LOW 1    // 1 = stisknuto = LOW (s INPUT_PULLUP)

// ---------------------------------------------------------------------------
//  Dotyk - kapacitni FT5436 (kompatibilni s knihovnou FT6236) po I2C.
//  Knihovnu FT6236.h / FT6236.cpp zkopirujte z LaskaKit ESPD-35 (adresar SW/,
//  stejne misto jako Touch_example.ino) do slozky sketche.
//
//  !!! DOPLNTE I2C piny z LaskaKit setup souboru pro TFT_eSPI !!!
//      (Setup303_ILI9488_ESPD-3_5_v3.h nebo Setup300_..._v2.h) - hledejte
//      radky "#define I2C_SDA ..." a "#define I2C_SCL ...".
// ---------------------------------------------------------------------------
#define TOUCH_ENABLE     1     // 0 = dotyk vypnout (jen tlacitko)
#define I2C_SDA          42    // ESPD-3.5 v3 (ESP32-S3)
#define I2C_SCL          2     // ESPD-3.5 v3 (ESP32-S3)
#define TOUCH_INT        -1    // volitelne (polling ho nepotrebuje)
#define TOUCH_SENSITIVITY 40   // citlivost FT6236 (nizsi = citlivejsi)
// Rotace dotyku, aby souradnice sedely s displejem (LCD_ROTATION 1):
//   3 = deska v2.1 a vyssi (FT5436),  1 = v2 a starsi (FT6234)
#define TOUCH_ROTATION   3

// ---------------------------------------------------------------------------
//  Rozvrzeni obrazovky: 3/4 mapa vlevo, 1/4 detailovy panel vpravo.
// ---------------------------------------------------------------------------
#define PANEL_W   124                         // sirka detailoveho panelu (~1/4)
#define MAP_W     (LCD_WIDTH - PANEL_W)        // sirka mapy (~3/4) = 356
#define MAP_H     LCD_HEIGHT                   // vyska mapy = 320
#define MAP_CX    (MAP_W / 2)                  // stred mapy X (poloha uzivatele)
#define MAP_CY    (MAP_H / 2)                  // stred mapy Y
// Polomer krouzku rozsahu v px. Vejde se na vysku mapy s malym okrajem.
// Definuje MERITKO:  scale [px/km] = R_RADIUS / rozsah_km.
// Svisly polomer = rozsah_km; vodorovne se diky obdelniku ukaze o neco vic.
#define R_RADIUS  (MAP_H / 2 - 8)

// ---------------------------------------------------------------------------
//  Casova zona a NTP.
// ---------------------------------------------------------------------------
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"
