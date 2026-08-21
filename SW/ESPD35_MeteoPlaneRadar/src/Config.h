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

// Jak dlouheho TICHA je potreba, nez se dotek povazuje za ukonceny.
//
// FT5436 obcas uprostred tazeni vzorek vynecha a dalsi se zahazuji uz ve
// filtrech v Touch_FT6236.cpp. Kdyby gesto koncilo hned na prvnim prazdnem
// vzorku, jeden swipe by se rozpadl na nekolik klepnuti do mapy. Skutecna
// gesta trvaji 40 ms a vic, takze 60 ms nic nestoji.
#define TOUCH_RELEASE_MS 60

// Jak casto se behem doteku cte radic (ms). Snimani dotyku bezi nove i uvnitr
// stahovani (viz netPoll v .ino), kde by se jinak volalo tisickrat za sekundu
// a zbytecne zatezovalo I2C sbernici.
#define TOUCH_PUMP_MS 5

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
// Hodnota 0 je zvlastni - znamena pevny pohled na celou CR (viz CZ_VIEW_*
// na konci souboru). Nechte ji posledni, je ze vsech nejsirsi.
#define METEO_RANGES_KM { 25.0f, 50.0f, 100.0f, 200.0f, 0.0f }

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

// =============================================================================
//  Pridano ve verzi 0.4.0
// =============================================================================

// ---------------------------------------------------------------------------
//  Sit
// ---------------------------------------------------------------------------
// TLS handshake potrebuje zhruba 45 kB INTERNI RAM (PSRAM se na to pouzit
// neda). Kdyz se zacne s mensi rezervou, selze hluboko uvnitr mbedTLS a navenek
// to vypada jako hole "HTTP -1". Radeji tedy poll preskocit a zkusit pozdeji -
// na displeji zustanou predchozi data.
//
// Bude to potreba hlavne od verze 0.5.0, kdy na pozadi pobezi web server.
#define NET_MIN_HEAP 60000

// ---------------------------------------------------------------------------
//  Nouzove kody odpovidace (squawk)
//  7500 unos, 7600 vypadek radia, 7700 obecna nouze.
//  Drzi se jako TEXT, ne cislo: jsou to osmickove kody a "0021" nesmi
//  zdegenerovat na 21.
// ---------------------------------------------------------------------------
#define SQUAWK_HIJACK "7500"
#define SQUAWK_RADIO  "7600"
#define SQUAWK_EMERG  "7700"

// ---------------------------------------------------------------------------
//  Nocni rezim (jas)
//
//  Automatiku podle vychodu a zapadu slunce prinese az verze 0.6.0 spolu
//  s predpovedi (casy slunce prijdou v teze odpovedi, takze nestoji zadny
//  pozadavek navic). Uz ted se ale ukladaji DVE urovne jasu, aby se pri
//  aktualizaci nemenily klice v NVS podruhe.
// ---------------------------------------------------------------------------
#define BRIGHT_DAY_DEFAULT     80   // %
#define BRIGHT_NIGHT_DEFAULT   25   // %
#define NIGHT_OFFSET_MIN_LIMIT 120  // +/- minut kolem vychodu a zapadu

// ---------------------------------------------------------------------------
//  Filtry letadel
//
//  Vychozi hodnoty zamerne nefiltruji nic - kdo si nic nenastavi, vidi tentyz
//  radar jako pred aktualizaci.
// ---------------------------------------------------------------------------
#define ALT_MIN_FT_DEFAULT      0
#define ALT_MAX_FT_DEFAULT  60000   // nad tim uz nic bezne nelita

// ---------------------------------------------------------------------------
//  Kontrola rozvrzeni obrazovky
//
//  1 = pri startu projit pevne prvky obrazovek a vypsat pripadny prekryv na
//  seriovou linku. Za behu nic nestoji a odhali preklep v konstante driv, nez
//  se z nej stanou dva popisky pres sebe.
// ---------------------------------------------------------------------------
#define LAYOUT_DEBUG 0

// ---------------------------------------------------------------------------
//  Diagnostika (seriova linka 115200 Bd)
// ---------------------------------------------------------------------------
// 1 = vypisovat kazde dokoncene gesto, kazdou zmenu vyberu letadla vcetne
//     DUVODU zavreni detailu a pocet zahozenych vadnych cteni dotyku.
//     Podle toho jde odlisit falesny dotyk od vypadku dat.
#define TOUCH_DEBUG 0

// =============================================================================
//  Pridano ve verzi 0.5.0 - webova konfigurace
// =============================================================================

// ---------------------------------------------------------------------------
//  Webovy konfiguracni server
//
//  Jeden server na portu 80 ve dvou rolich: dokud nejsou ulozene udaje k WiFi,
//  je to captive portal na vlastnim pristupovem bode; potom bezi TRVALE na
//  domaci siti. Ani jedno nema timeout - stranka, kterou musite jit nejdriv
//  zapnout na zarizeni, je stranka, kterou prestanete pouzivat.
// ---------------------------------------------------------------------------
#define WEB_PORT       80
#define WEB_HOSTNAME   "espd35meteoradar"   // -> http://espd35meteoradar.local/
#define WEB_ADMIN_USER "admin"

// Jak dlouho se ceka na pripojeni k ulozene siti, nez se vzda a vyskoci AP.
#define WIFI_CONNECT_TIMEOUT_MS 20000UL
// Jak casto zkouset znovu, kdyz spojeni behem provozu spadne.
#define WIFI_RETRY_MS           15000UL

// =============================================================================
//  Pridano ve verzi 0.6.0 - hodiny, predpoved, pet obrazovek
// =============================================================================

// ---------------------------------------------------------------------------
//  Obrazovky
//
//  Pet, a ctyri datove se daji jednotlive vypnout ve webovem rozhrani.
//  Nastaveni vypnout nejde - bez nej by se uz nedalo dostat zpet k webu, kdyby
//  nekdo vypnul vsechno ostatni.
//
//  POZOR na cislovani: do 0.5.0 platilo LETADLA=0, METEO=1, NASTAVENI=2 a tahle
//  cisla jsou ulozena v NVS pod klicem "scr". Nove cislovani je jine, proto se
//  posledni obrazovka uklada pod NOVY klic "scr2" a stary se pri prvnim startu
//  jednorazove prevede (viz Settings_Begin).
// ---------------------------------------------------------------------------
#define SCREEN_CLOCK_I    0
#define SCREEN_PLANES_I   1
#define SCREEN_METEO_I    2
#define SCREEN_FORECAST_I 3
#define SCREEN_SETTINGS_I 4
#define SCREEN_N          5

// Automaticke stridani obrazovek se pozastavi na tuhle dobu po gestu, ktere
// znamena "ridim to sam" (swipe, dlouhy stisk). Klepnuti ho nezastavuje -
// byt prepnut uprostred cteni je otravne, ale stejne tak zarizeni, ktere
// prestane stridat, protoze nekdo zavadil o sklo.
#define AUTO_ROTATE_PAUSE_MS 600000UL

// ---------------------------------------------------------------------------
//  Predpoved, vychod/zapad slunce a ovzdusi (vse Open-Meteo, zdarma, bez klice)
//
//  JEDEN pozadavek obsluhuje tri veci: obrazovku predpovedi, aktualni teplotu
//  na hodinach a casy slunce pro nocni rezim. Proto jsou v jednom modulu.
// ---------------------------------------------------------------------------
#define FORECAST_URL   "https://api.open-meteo.com/v1/forecast"
#define AIRQUALITY_URL "https://air-quality-api.open-meteo.com/v1/air-quality"
#define GEOCODE_URL    "https://geocoding-api.open-meteo.com/v1/search"

#define FORECAST_HOURS 6        // hodinove sloupce (480 / 6 = 80 px na sloupec)
#define FORECAST_DAYS  3        // denni radky pod nimi
#define FORECAST_PERIOD_MS 1800000UL   // 30 min
#define FORECAST_RETRY_MS   120000UL   // 2 min po neuspechu

#define AQ_PERIOD_MS 1800000UL
#define AQ_RETRY_MS   120000UL

// ---------------------------------------------------------------------------
//  Venkovni teplota (radek vedle hodin)
//
//  Znaminko stupnu: vestaveny font je 7bitove ASCII, takze skutecny "°" muze
//  vyjit jako nahodny znak. "degC" je bezpecny zapis. Prepnout na 1 az potom,
//  co si na skutecnem displeji overite, ze se znak vykresli spravne.
// ---------------------------------------------------------------------------
#define OUTSIDE_DEG_SYMBOL 0
#if OUTSIDE_DEG_SYMBOL
  #define OUTSIDE_DEG_TEXT "\xB0C"
#else
  #define OUTSIDE_DEG_TEXT "degC"
#endif

// ---------------------------------------------------------------------------
//  Trasa letu (adsbdb.com) - zdarma, bez klice, bez registrace.
//  Pta se JEN pri otevreni detailu letadla, na jedno letadlo, a odpoved se
//  kesuje - preblikavani mezi dvema letadly uz API nezatezuje.
// ---------------------------------------------------------------------------
#define ROUTE_API_BASE "https://api.adsbdb.com"
#define ROUTE_CACHE_N  8      // zapamatovanych odpovedi

// ---------------------------------------------------------------------------
//  Obrazovka hodin - beh sekund
//
//  Puvodne to byl beh po samem OBVODU ramu. V krabicce to nefungovalo: rameček
//  krabicky prekryva par pixelu po okrajich a beh byl temer neviditelny.
//  Od 0.4.0 je to proto ELIPSA vepsana do displeje s odsazenim od kraju -
//  cely beh je uvnitr viditelne plochy a zaroven to lip odpovida tvaru
//  puvodniho prstence z kulateho displeje.
//
//  Odsazeni radeji vetsi nez mensi: par pixelu navic uvnitr nikoho nerusi,
//  kdezto beh schovany pod krabickou je k nicemu.
// ---------------------------------------------------------------------------
#define SEC_STYLE_OFF   0
#define SEC_STYLE_DOTS  1     // 60 znacek po elipse
#define SEC_STYLE_LINE  2     // "Cara" - plny oblouk od dvanactky
#define SEC_STYLE_COMET 3     // "Kometa" - bezec s dohasinajicim ohonem

#define CLK_SEC_TH        4   // tloustka behu sekund v px
#define CLK_SEC_INSET_X  16   // odsazeni elipsy od leveho a praveho kraje
#define CLK_SEC_INSET_Y  16   // odsazeni od horniho a dolniho kraje

// Delka ohonu komety v sekundach (tedy jak velky kus elipsy za hlavou sviti).
// 20 s je tretina otacky - dost na to, aby byl smer pohybu zrejmy na prvni
// pohled, a zaroven ne tolik, aby splynul s plnym kruhem.
#define CLK_COMET_TAIL_SEC 20

// ---------------------------------------------------------------------------
//  Meteoradar: pohled na CELOU CR
//
//  Posledni rozsah (hodnota 0 v METEO_RANGES_KM) je zvlastni: znamena pevny
//  vyrez celeho statu bez ohledu na to, kde uzivatel je. Zadava se jako STRED
//  a POLOMER, ne jako obdelnik - obrazek CHMU ma na kazde ose jine meritko,
//  takze ram, ktery vypada ctvercove v pixelech, ctvercovy NA ZEMI neni
//  a stat by se svisle protahl. Pruchodem tymz vypoctem polomeru jako u vsech
//  ostatnich rozsahu zustane meritko poctive.
//
//  Republika je 486 x 279 km, takze 260 km polomeru ji pokryje s rezervou.
// ---------------------------------------------------------------------------
#define CZ_VIEW_LAT       49.805f
#define CZ_VIEW_LON       15.475f
#define CZ_VIEW_RADIUS_KM 260.0f
