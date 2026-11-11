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
#include "Version.h"   // FW_VERSION - jde do hlavicky User-Agent nize

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
//  Funkce prevzate z petus/MeteoPlaneRadar 0.8.0 - 0.8.5
// ---------------------------------------------------------------------------

// --- Zdroj meteoradaru ------------------------------------------------------
// CHMU, nebo tentyz snimek "masked" (jen srazky, ktere dopadaji na zem).
// RainViewer na teto desce neni; hodnota 1 je vyhrazena kvuli kompatibilite
// exportu s puvodnim projektem a chova se jako CHMU.
#define RADAR_SRC_CHMU        0
#define RADAR_SRC_RAINVIEWER  1     // jen rezervovano
#define RADAR_SRC_CHMU_MASKED 2
#define RADAR_SRC_MAX         2

// --- Pismo (0.8.0 hodiny, 0.8.5 vsechny obrazovky) --------------------------
// Vyhlazene obrysy pisma (Inter, Poppins) vykreslene v 16 urovnich pokryti, viz
// AAFont.h a tools/make_fonts.py v puvodnim projektu. CLOCK_FONT_PIXEL vrati
// vestaveny font 5x7 a vsechno vypada jako drive.
#define CLOCK_FONT_PIXEL   0
#define CLOCK_FONT_INTER   1     // Inter Display Medium - vychozi
#define CLOCK_FONT_POPPINS 2     // Poppins Medium - kulatejsi
#define CLOCK_FONT_THIN    3     // Inter Display Light - tenke
#define CLOCK_FONT_MAX     3
#define CLOCK_FONT_DEFAULT CLOCK_FONT_INTER

// --- Barvy vysek letadel (0.8.0) --------------------------------------------
// Ctyri hranice ve STOPACH a pet barev. Vychozi hodnoty jsou presne dosavadni
// legenda teto desky (<3, 3-10, 10-20, 20-30, >30 tisic ft), takze dokud se do
// nastaveni nesahne, nic se nemeni.
#define ALT_BANDS_N          4
#define ALT_BAND0_FT_DEFAULT  3000
#define ALT_BAND1_FT_DEFAULT 10000
#define ALT_BAND2_FT_DEFAULT 20000
#define ALT_BAND3_FT_DEFAULT 30000
#define ALT_COL0_DEFAULT 0xF800       // cervena - priblizeni, vrtulniky
#define ALT_COL1_DEFAULT 0xFC00       // oranzova - stoupani / klesani
#define ALT_COL2_DEFAULT 0xFFE0       // zluta - stredni hladiny
#define ALT_COL3_DEFAULT 0x07E0       // zelena - nizsi cestovni
#define ALT_COL4_DEFAULT 0x02DF       // modra - cestovni hladina

// --- Hlidani bourky (0.8.0) -------------------------------------------------
// Kazdych STORM_PERIOD_MS se zkontroluje nejnovejsi snimek CHMU: je do zadane
// vzdalenosti odraz aspon zadane sily? Kdyz ano, meteoradar se prida do
// stridani obrazovek.
#define STORM_DBZ_DEFAULT   20
#define STORM_KM_DEFAULT    50
#define STORM_PERIOD_MS     300000UL   // 5 min, kadence CHMU
#define STORM_RETRY_MS       90000UL   // drive po neuspechu
// Jeden pixel je ctverecni kilometr a muze byt artefakt radaru.
#define STORM_MIN_PIXELS    3
// Kolik klidnych snimku po sobe vyradi meteoradar zase ze stridani.
#define STORM_CLEAR_CHECKS  2

// --- Letadlo poblic a vyhled z okna (0.8.0) --------------------------------
#define PLANE_ALERT_KM_DEFAULT   5
#define PLANE_ALERT_SEC_DEFAULT  15
// Jak casto se ptat adsb.fi, kdyz obrazovka Letadla NENI na displeji. Ptame se
// jen na maly okruh, odpoved ma par set bajtu.
#define PLANE_WATCH_PERIOD_MS    8000UL
// Jedno letadlo se ohlasi jednou; pamatuje se tak dlouho.
#define PLANE_ALERT_FORGET_MS    900000UL   // 15 min
#define PLANE_ALERT_MEMORY       12
#define WINDOW_WIDTH_DEFAULT      90
#define WINDOW_KM_DEFAULT         15

// --- WiFi: az tri site (0.8.0) ----------------------------------------------
#define WIFI_SLOTS 3

// --- Jazyk (displej i web) --------------------------------------------------
#define LANG_CS      0
#define LANG_SK      1
#define LANG_MAX     1
#define LANG_DEFAULT LANG_CS
// Pri vypadku se nejdriv zkousi sit, na ktere deska byla (restart routeru je
// obvykla pricina), a teprve po teto dobe ostatni ulozene.
#define WIFI_SWITCH_AFTER_MS 60000UL
#define WIFI_TRY_MS          12000UL    // na jednu sit pri startu
// Po restartu watchdogem (nebo padu) deska routeru neohlasi odpojeni a router
// ji muze chvili odmitat, dokud stare spojeni sam nezahodi. Proto se pri startu
// ulozene site zkousi vic kol (dalsi kola s delsim cekanim), nez vyskoci portal.
#define WIFI_BOOT_ROUNDS     3
// Portal (QR s vyberem site) pri ulozene siti: kdyz se k pristupovemu bodu
// za tuto dobu nikdo nepripoji, deska se restartuje a zkusi ulozene site znovu.
// Bez ulozene site se nerestartuje - neni co zkouset. 0 = vypnuto.
#define WIFI_PORTAL_RESTART_MS (30UL * 60UL * 1000UL)

// --- Home Assistant (0.8.0) -------------------------------------------------
// GET http://<deska>/api/ha vraci jeden plochy JSON pro integraci RESTful.
// Ve vychozim stavu vypnuto. NEOVERENO se skutecnym Home Assistantem.
#define HA_DEFAULT_ON false

// ---------------------------------------------------------------------------
//  Vypadek adsb.fi (z petus/MeteoPlaneRadar 0.8.4)
//
//  Drive se po chybe zeptalo hned znovu a pak dal kazdych 10-30 s, coz je proti
//  pretizenemu serveru presne spatne. Ted kazda dalsi chyba v rade zdvojnasobi
//  spolecnou pauzu (radar, hlidani letadel poblic i Home Assistant chodi pres
//  stejnou branu) az do ADSB_BACKOFF_MAX_MS. 429 a 5xx se v temze pokusu
//  neopakuji a Retry-After od serveru se respektuje. Prvni dobra odpoved pauzu
//  zrusi.
//
//  Stara letadla se nevydavaji za ziva: po ADSB_STALE_MS bez cerstvych dat
//  zesednou a misto poctu se napise jejich stari, po ADSB_HIDE_MS se
//  nekresli vubec. Ze starych dat se nespusti ani upozorneni.
// ---------------------------------------------------------------------------
#define ADSB_BACKOFF_BASE_MS   10000UL     // pauza po prvni chybe
#define ADSB_BACKOFF_MAX_MS   300000UL     // nejvys 5 min
#define ADSB_RETRY_AFTER_MAX_S   900       // Retry-After brat vazne do 15 min
#define ADSB_STALE_MS          60000UL     // sede po minute
#define ADSB_HIDE_MS          600000UL     // pryc po deseti

// ---------------------------------------------------------------------------
//  Meteoradar (CHMU)
// ---------------------------------------------------------------------------
// Hodnota 0 je zvlastni - znamena pevny pohled na celou CR (viz CZ_VIEW_*
// na konci souboru). Nechte ji posledni, je ze vsech nejsirsi.
#define METEO_RANGES_KM { 25.0f, 50.0f, 100.0f, 200.0f, 0.0f }

// Obnova animace: po uspechu jednou za pet minut (CHMU vydava snimek po peti
// minutach), po neuspechu uz za minutu. Drive se po neuspechu index zkousel
// v kazdem pruchodu smyckou, tedy prakticky porad dokola.
#define METEO_PERIOD_MS 300000UL
#define METEO_RETRY_MS   60000UL

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

// WiFiClientSecure necha mbedTLS na handshake vychozich 120 s, tedy
// sestinasobek WDT_TIMEOUT_S. setConnectTimeout() to NEKRYJE - ten omezuje jen
// navazani TCP spojeni. Smycka handshaku ma vlastni limit a bezi uvnitr
// http.GET(), kde watchdog nikdo neresetuje. V sekundach, ne v ms.
#define NET_TLS_HANDSHAKE_S 8

// Strop na cas jednoho stahovaneho tela. HTTPClient::writeToStreamDataBlock()
// se toci na delay(1) bez vlastniho timeoutu, takze server, ktery prestane
// posilat a pritom drzi socket otevreny, by visel az do hardwaroveho watchdogu.
// Hlida to proto sink a na displeji zustanou predchozi data.
#define NET_BODY_BUDGET_MS 15000UL

// Strop na TEXTOVOU odpoved (Net_GetString). Telo se cte do sdileneho bufferu
// v PSRAM - drive ho bral http.getString() bez jakehokoli stropu, a to na
// interni heap, ze ktereho si bere i TLS handshake. Nejvetsi ocekavane telo je
// cena elektriny na dva dny po ctvrthodinach (~25 kB); 128 kB je bohata rezerva.
#define NET_MAX_TEXT (128 * 1024)

// Hlavicka User-Agent pro VSECHNY odchozi dotazy. Neni to jen zdvorilost:
// adsb.lol odpovi 403 s telem "User-Agent too generic; include valid contact
// info", kdyz v ni zadny kontakt nevidi - a presne to delala vychozi
// "ESP32HTTPClient", ktera odchazela, dokud se hlavicka omylem nastavovala
// pres addHeader() (to ji tise zahazuje, viz ADSB.cpp). Odkaz na web projektu
// jako kontakt staci. Kdyz projekt forknete, dejte sem SVUJ.
#define HTTP_USER_AGENT "ESPD35_MeteoPlaneRadar/" FW_VERSION " (+https://chiptron.cz)"

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
//  Sedm, a sest datovych se da jednotlive vypnout ve webovem rozhrani.
//  Nastaveni vypnout nejde - bez nej by se uz nedalo dostat zpet k webu, kdyby
//  nekdo vypnul vsechno ostatni.
//
//  POZOR na cislovani: do 0.5.0 platilo LETADLA=0, METEO=1, NASTAVENI=2 a tahle
//  cisla jsou ulozena v NVS pod klicem "scr". Pak prisly hodiny a predpoved
//  (klic "scr2") a s energetickymi obrazovkami se Nastaveni posunulo ze 4 na 6
//  (klic "scr3"). Starsi klice se pri prvnim startu jednorazove prevedou (viz
//  Settings_Begin).
//
//  Obe energeticke obrazovky jsou po aktualizaci VYPNUTE - zapinaji se ve
//  webovem nastaveni. Dokud jsou vypnute, deska si o jejich data vubec nerekne.
// ---------------------------------------------------------------------------
#define SCREEN_CLOCK_I    0
#define SCREEN_PLANES_I   1
#define SCREEN_METEO_I    2
#define SCREEN_FORECAST_I 3
#define SCREEN_PRICE_I    4     // spotova cena elektriny
#define SCREEN_MIX_I      5     // vyroba elektriny podle zdroju
#define SCREEN_SETTINGS_I 6
#define SCREEN_N          7

// Vychozi maska zapnutych datovych obrazovek: hodiny, letadla, meteoradar,
// predpoved. Energeticke obrazovky si uzivatel zapne sam.
#define SCREEN_MASK_DEFAULT 0x0F
#define SCREEN_MASK_ALL     0x3F

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

// Predpoved se cte po radcich: "nyni", pak dve okna po FORECAST_WIN_H hodinach
// popsana casem (12-15h, 15-18h), pod nimi dnesek a dalsi dny s datem.
// FORECAST_HOURS je pocet HODINOVYCH hodnot, ktere se stahnou: hodina, ve
// ktere jsme, plus dve okna.
#define FORECAST_WIN_H 3
#define FORECAST_HOURS (1 + 2 * FORECAST_WIN_H)   // 7
// Dnesek + dalsi dny. Na 320 px vysky se jich pod okna vejde pet.
#define FORECAST_DAYS  5
#define FORECAST_PERIOD_MS 1800000UL   // 30 min
#define FORECAST_RETRY_MS   120000UL   // 2 min po neuspechu

#define AQ_PERIOD_MS 1800000UL
#define AQ_RETRY_MS   120000UL

// ---------------------------------------------------------------------------
//  Venkovni teplota (radek vedle hodin)
//
//  Znaminko stupnu. Vestaveny font (glcdfont) je razeny podle CP437 a stupen
//  v nem lezi na 0xF8 - Arduino_GFX ho indexuje primo, bez posunu, ktery
//  zna Adafruit GFX. Drive se sahalo na 0xB0 (Latin-1), kde je ve fontu
//  stinovany blok, a proto byl vychozi nouzovy zapis "degC". Prepnutim na 0
//  se "degC" vrati.
// ---------------------------------------------------------------------------
#define OUTSIDE_DEG_SYMBOL 1
#if OUTSIDE_DEG_SYMBOL
  #define OUTSIDE_DEG_TEXT "\xF8" "C"
#else
  #define OUTSIDE_DEG_TEXT "degC"
#endif

// ---------------------------------------------------------------------------
//  Energie: spotova cena elektriny a vyroba podle zdroju
//
//  Oba zdroje zdarma, bez klice a bez registrace - stejna latka jako u vsech
//  ostatnich zdroju v projektu.
//
//  CENA: spotovaelektrina.cz preposila vysledek denniho trhu OTE uz v Kc,
//  dnesek i zitrek v JEDNE odpovedi. Endpoint -qh vraci CTVRTHODINY - v nich
//  se na OTE doopravdy obchoduje. Starsi get-prices-json vraci hodinove
//  PRUMERY a vlastni dokumentace API ho vede jako zastaraly; na odpolednim
//  nabehu se ctvrthodina od prumeru hodiny lisi i o pul koruny za kWh.
//
//    {"hoursToday":[{"hour":0,"minute":0,"priceCZK":5291, ...}, ...],
//     "hoursTomorrow":[ ... ]}
//
//  96 polozek na den, priceCZK je za MEGAWATTHODINU. Vsechno pod stahovanim
//  pracuje v Kc/MWh a teprve obrazovka deli tisicem. hoursTomorrow je prazdne,
//  dokud se aukce neuzavre (kolem 14:00) - to je bezny stav, ne chyba.
//
//  VYROBA: rozhrani energy-charts (Fraunhofer ISE) s daty ENTSO-E, bez tokenu,
//  ktery vyzaduje samotne ENTSO-E.
//
//    {"unix_seconds":[...], "production_types":[{"name":"Nuclear","data":[...]}]}
//
//  Zalezi na okne. Za cely den prijde pres sto kilobajtu; za posledni par
//  hodin par kilobajtu - a potrebujeme jen nejnovejsi vzorek.
// ---------------------------------------------------------------------------
#define PRICE_URL "https://spotovaelektrina.cz/api/v1/price/get-prices-json-qh"
#define MIX_URL   "https://api.energy-charts.info/public_power"

// Vychozi zeme pro vyrobu. Jen vychozi - skutecna je v nastaveni
// (Settings_MixCountry), energy-charts pokryva 38 evropskych zemi a "eu".
#define MIX_COUNTRY_DEFAULT "cz"

// Kde maji obe obrazovky smysl. Ceny jsou cesky trh v Kc, vyroba funguje
// v Evrope. Mimo tyto obdelniky obrazovka rekne, ze data nema, a stahovani
// mlci. Obdelniky, ne hranice: u hranic radeji obrazovku ukazat (nekdo
// v Zitave muze ceske ceny chtit), a jedno zaskrtnuti ve webu to stejne vyresi.
#define PRICE_AREA_LAT_MIN  48.3f
#define PRICE_AREA_LAT_MAX  51.3f
#define PRICE_AREA_LON_MIN  11.8f
#define PRICE_AREA_LON_MAX  19.2f

#define MIX_AREA_LAT_MIN    34.0f
#define MIX_AREA_LAT_MAX    72.0f
#define MIX_AREA_LON_MIN   -25.0f
#define MIX_AREA_LON_MAX    45.0f

// Jak daleko zpet se ptat. Data se OBVYKLE zverejnuji se zpozdenim jedne az dvou
// hodin - ale ne vzdy: 25. 10. 2026 byl nejnovejsi cesky vzorek stary jedenact
// hodin, ctyrhodinove okno prislo jako "404 no content available" a obrazovka
// cely den visela na "Nacitam". Okno se proto rozsiruje, dokud neco neprijde.
// Uzke jde prvni, protoze ma par kilobajtu.
#define MIX_WINDOW_H      4
#define MIX_WINDOWS_H     { MIX_WINDOW_H, 12, 36 }

// Cena je na cely den pevna, jde tedy hlavne o zitrek, ktery vychazi kolem 14:00.
#define PRICE_PERIOD_MS  1800000UL   // 30 min
#define PRICE_RETRY_MS    120000UL   // 2 min po neuspechu

// Vyroba je u zdroje po ctvrthodinach - casteji nema smysl.
#define MIX_PERIOD_MS     900000UL   // 15 min
#define MIX_RETRY_MS      120000UL

// Strop barevne skaly ceny v Kc/MWh. Pod nim se barvy natahuji pres rozsah
// dne (aby byl citelny i plochy den), nad nim je skala pribita - drahy den
// ma vypadat drahy, ne jen "cerveny na svem vrcholu".
#define PRICE_SCALE_MAX 6000.0f

// Zaporna cena neni chyba - nastava o vetrnych nedelich a je presne to, o cem
// chcete vedet. Kresli se vlastni barvou.
#define PRICE_NEGATIVE_BELOW 0.0f

// O kolik se smi soucet vyroby lisit od spotreby + vyvozu, nez stavova stranka
// rekne, ze mix vypada neuplne (chybejici typ zdroje).
#define MIX_SANITY_TOL 0.12f

// ---------------------------------------------------------------------------
//  Trasa letu (adsb.lol) - zdarma, bez klice, bez registrace.
//  Pta se JEN pri otevreni detailu letadla, na jedno letadlo, a odpoved se
//  kesuje - preblikavani mezi dvema letadly uz API nezatezuje.
//  Cela cesta je BASE/{callsign}/{lat}/{lon} - poloha jde s dotazem, aby server
//  mohl rict, jestli trasa k mistu, kde letadlo opravdu je, vubec sedi
//  (viz Route.h).
// ---------------------------------------------------------------------------
#define ROUTE_API_BASE "https://api.adsb.lol/api/0/route"
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
