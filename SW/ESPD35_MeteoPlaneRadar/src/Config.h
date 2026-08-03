// =============================================================================
//  ESPD35_MeteoPlaneRadar - radar letadel (adsb.fi) + meteoradar CHMU
//  na desce LaskaKit ESPD-3.5" (ESP32-S3, ILI9488 480x320 SPI, dotyk FT5436).
//
//  >>> TENTO SOUBOR JE JEDINY, KTERY VETSINOU MUSITE UPRAVIT <<<
//  Krome pinu jsou tu VSECHNY laditelne konstanty: casova zona, vychozi poloha,
//  rozsahy, intervaly stahovani, nazev AP, limity, ladici prepinace.
//  Vse je jen prekladovy vychozi stav - poloha, jas, jednotky, posledni
//  obrazovka a rozsah se za behu ukladaji do NVS (viz Settings.*).
//
//  Port projektu petus/MeteoPlaneRadar (kulaty 480x480) na obdelnikovy
//  displej ILI9488 480x320 (SPI), ESP32-S3.
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
//  SPI piny displeje ILI9488 (overeno na ESPD-3.5 Rev. 3.2 / ESP32-S3).
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
//  POZNAMKA: ESPD-3.5 nema zadne uzivatelske tlacitko. Cele ovladani je
//  dotykove. Tovarni reset je proto tlacitkem na obrazovce Nastaveni
//  (s potvrzenim - viz RESET_CONFIRM_MS nize).
// ---------------------------------------------------------------------------

// Jak dlouho po prvnim klepnuti na "Tovarni reset" plati nabidka potvrzeni.
// Kdyz uzivatel do teto doby neklepne podruhe, tlacitko se vrati do klidu.
#define RESET_CONFIRM_MS 6000UL

// ---------------------------------------------------------------------------
//  Dotyk - kapacitni FT5436 (kompatibilni s knihovnou FT6236) po I2C.
// ---------------------------------------------------------------------------
#define TOUCH_ENABLE     1     // 0 = dotyk vypnout (jen tlacitko)
#define I2C_SDA          42    // ESPD-3.5 v3 (ESP32-S3)
#define I2C_SCL          2     // ESPD-3.5 v3 (ESP32-S3)
#define TOUCH_INT        -1    // volitelne (polling ho nepotrebuje)
#define TOUCH_SENSITIVITY 40   // citlivost FT6236 (nizsi = citlivejsi)
// Rotace dotyku, aby souradnice sedely s displejem (LCD_ROTATION 1):
//   3 = deska v2.1 a vyssi (FT5436),  1 = v2 a starsi (FT6234)
#define TOUCH_ROTATION   3

// Maximalni skok mezi dvema po sobe jdoucimi vzorky BEHEM jednoho doteku (px).
// Prst se za 5 ms takhle daleko neposune - vetsi skok je porucha cteni (viz
// Touch_FT6236.cpp) a vzorek se zahodi. 0 = filtr vypnout.
#define TOUCH_MAX_JUMP_PX 160

// ---------------------------------------------------------------------------
//  Rozvrzeni obrazovky letadel: 3/4 mapa vlevo, 1/4 detailovy panel vpravo.
// ---------------------------------------------------------------------------
#define PANEL_W   124                          // sirka detailoveho panelu (~1/4)
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
#define TZ_INFO    "CET-1CEST,M3.5.0,M10.5.0/3"
#define NTP_SERVER "pool.ntp.org"

// ---------------------------------------------------------------------------
//  Vychozi poloha (Praha). Prepise se pri prvnim startu geolokaci podle IP
//  nebo rucne ve WiFi portalu; ulozena hodnota ma vzdy prednost.
// ---------------------------------------------------------------------------
#define DEFAULT_LAT 50.0755
#define DEFAULT_LON 14.4378

// ---------------------------------------------------------------------------
//  Konfiguracni pristupovy bod (WiFi portal i OTA pouzivaji stejny nazev).
//  Max. 32 znaku.
// ---------------------------------------------------------------------------
#define AP_SSID     "ESPD35-MeteoPlaneRadar"
#define AP_PASSWORD ""     // "" = otevrena sit

// ---------------------------------------------------------------------------
//  Radar letadel (adsb.fi)
// ---------------------------------------------------------------------------
#define ADSB_MAX 100       // strop letadel drzenych/kreslenych (jen ve vzduchu)

// Volitelne rozsahy v km (vzestupne; pocet se dopocita).
#define PLANE_RANGES_KM { 10.0f, 25.0f, 50.0f, 100.0f }

// Perioda stahovani podle rozsahu. Vetsi okruh vraci vic dat a je mene casove
// kriticky, takze se stahuje redceji - setrnejsi k bezplatnemu API adsb.fi.
// Po neuspesnem stazeni se interval zdvojnasobi.
#define ADSB_PERIOD_NEAR_MS  5000    // do  ADSB_NEAR_KM
#define ADSB_PERIOD_MID_MS  10000    // do  ADSB_MID_KM
#define ADSB_PERIOD_FAR_MS  15000    // nad ADSB_MID_KM
#define ADSB_NEAR_KM 25.0f
#define ADSB_MID_KM  50.0f

// ---------------------------------------------------------------------------
//  Meteoradar (CHMU)
// ---------------------------------------------------------------------------
#define METEO_RANGES_KM { 25.0f, 50.0f, 100.0f, 200.0f }

// ---------------------------------------------------------------------------
//  Detail letadla
// ---------------------------------------------------------------------------
// adsb.fi obcas letadlo v jednom stazeni vynecha a v dalsim ho zase posle.
// Kdyby se detail zavrel hned pri prvnim vypadku, vypada to, ze se zavira sam.
// Tolerujeme tedy tenhle pocet po sobe jdoucich chybejicich stazeni; behem nich
// panel drzi posledni zname hodnoty s poznamkou "signal ztracen".
#define DETAIL_GRACE_POLLS 2

// ---------------------------------------------------------------------------
//  Orientace mapy
//  Uzivatel voli, ktery svetovy SMER je NAHORE na radaru letadel - tedy smer,
//  kterym se diva z okna. Krok musi delit 90 beze zbytku, jinak prestanou byt
//  dosazitelne presny vychod a zapad.
// ---------------------------------------------------------------------------
#define MAP_ROT_STEP_DEG 45    // stupnu na jeden stisk (45 -> osm poloh)

// ---------------------------------------------------------------------------
//  OTA (aktualizace firmwaru pres WiFi)
// ---------------------------------------------------------------------------
#define OTA_IDLE_MS 300000UL   // po teto dobe bez nahravani se rezim OTA ukonci

// ---------------------------------------------------------------------------
//  Watchdog
// ---------------------------------------------------------------------------
#define WDT_TIMEOUT_S 20       // restart po tolika sekundach zaseknuti

// ---------------------------------------------------------------------------
//  Diagnostika (seriova linka 115200 Bd)
// ---------------------------------------------------------------------------
// 1 = vypisovat kazde dokoncene gesto, kazdou zmenu vyberu letadla vcetne
//     DUVODU zavreni detailu a pocet zahozenych vadnych cteni dotyku.
//     Podle toho jde odlisit falesny dotyk od vypadku dat.
#define TOUCH_DEBUG 0
