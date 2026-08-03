// =============================================================================
//  ESPD35_MeteoPlaneRadar - radar letadel (adsb.fi) + meteoradar CHMU
//  na desce LaskaKit ESPD-3.5"
// =============================================================================
//
//  Deska:   LaskaKit ESPD-3.5" (ESP32-S3, 16 MB flash), ILI9488 480x320 (SPI),
//           kapacitni dotyk FT5436 (I2C, knihovna FT6236)
//
//  Tri obrazovky (dokola):
//    1) LETADLA  - 3/4 mapa vlevo + 1/4 detail letadla vpravo
//    2) METEO    - celoobrazovkovy srazkovy radar CHMU s animaci
//    3) NASTAVENI- jas, orientace mapy, jednotky, WiFi + poloha, aktualizace FW
//
//  Ovladani je CELE dotykove - ESPD-3.5 nema zadne uzivatelske tlacitko:
//    - swipe vlevo/vpravo        = zmena rozsahu aktivni obrazovky
//    - kratke klepnuti na letadlo= zafixovat v detailu; do prazdna = zpet
//    - dlouhy stisk v LEVE pulce = predchozi obrazovka
//    - dlouhy stisk v PRAVE pulce= nasledujici obrazovka
//    - klepnuti na tecky dole    = skok primo na danou obrazovku
//    - tovarni reset             = tlacitko na obrazovce Nastaveni (2 klepnuti)
//
//  ORIENTACE MAPY (Nastaveni -> "Nahore"): nastavuje se smer, kterym se divate
//  z okna. Letadla na displeji jsou pak ve stejnem smeru jako ta za sklem.
//  Otaci se PROJEKCE, ne displej, takze se spravne otoci i obrys, mesta a ikony
//  letadel a dotykove souradnice zustavaji platne. Meteoradar se zamerne
//  neotaci - srazkova mapa se cte severem nahoru.
//
//  Port projektu petus/MeteoPlaneRadar (kulaty displej Waveshare 480x480).
//  Zachovana izotropni projekce -> spravne meritko a shoda dat s mesty
//  (letadla plocha azimutalni projekce, meteo Web Mercator).
//
//  Knihovny (Arduino IDE, ESP32 core 3.x):
//    - GFX Library for Arduino (moononournation)  - kresleni + canvas
//    - PNGdec (bitbank2)                          - dekodovani snimku CHMU
//    - ArduinoJson (bblanchon, v7)                - parsovani ADS-B dat
//    - WiFiManager (tzapu)                        - konfiguracni WiFi portal
//    - ElegantOTA (ayushsharma82)                 - aktualizace pres WiFi
//    - QRCode (ricmoo)                            - QR kod v AP portalu
//    - FT6236 (prilozena LaskaKit)                - kapacitni dotyk
//
//  POZOR: pro OTA je nutna vlastni tabulka oddilu (src/partitions.csv, dva
//  aplikacni sloty po 6 MB) - v Arduino IDE zvolte Partition Scheme = Custom.
//
//  Zdroje dat (nutno uvest, jen pro osobni nekomercni pouziti):
//    - Letadla: adsb.fi, https://adsb.fi
//    - Srazky:  Cesky hydrometeorologicky ustav (CHMU), https://opendata.chmi.cz
//    - Poloha:  ip-api.com (automaticka detekce dle IP)
//
//  Licence: MIT. Na displeji se zobrazuje napis "laskakit.cz".
//  Puvodni projekt petus/MeteoPlaneRadar vznikl pro chiptron.cz (atribuce).
//
//  Verze: src/Version.h (FW_VERSION).  Historie zmen: CHANGELOG.md.
// =============================================================================

#include <Arduino_GFX_Library.h>
#include <time.h>
#include "esp_arduino_version.h"

#include "Config.h"
#include "Version.h"
#include "UI.h"
#include "Settings.h"
#include "WiFiPortal.h"
#include "GeoIP.h"
#include "ADSB.h"
#include "ScreenPlanes.h"
#include "CHMU.h"
#include "ScreenWeather.h"
#include "ScreenSettings.h"
#include "OTA.h"
#include "Watchdog.h"
#if TOUCH_ENABLE
  #include "Touch_FT6236.h"
#endif

// --- Displej: ILI9488 (SPI) + off-screen canvas v PSRAM (proti blikani) ---
Arduino_DataBus* bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO);
Arduino_GFX*     panel = new Arduino_ILI9488_18bit(bus, TFT_RST, LCD_ROTATION, false /*IPS*/);

// gfx = canvas. Vsechno kresleni jde sem, flush() posle cely snimek najednou.
Arduino_GFX* gfx = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, panel);

// yield() pri dlouhych prenosech + nakrmi watchdog, aby stahovani ADS-B
// (bufferovane cteni + jeden retry) a meteoradaru nemohlo trefit WDT.
static void netPoll() { yield(); Watchdog_Feed(); }

// ---------------------------------------------------------------------------
//  Podsviceni (PWM), kompatibilni s core 2.x i 3.x.
//  Neni static - vola ho i obrazovka Nastaveni (posuvnik jasu). Deklarace
//  je v UI.h.
// ---------------------------------------------------------------------------
void Backlight_Set(uint8_t pct) {
  if (TFT_BL < 0) return;
  uint32_t duty = (uint32_t)pct * 255 / 100;
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcAttach(TFT_BL, 5000, 8);
  ledcWrite(TFT_BL, duty);
#else
  ledcSetup(0, 5000, 8);
  ledcAttachPin(TFT_BL, 0);
  ledcWrite(0, duty);
#endif
}

// ---------------------------------------------------------------------------
//  Sprava obrazovek
// ---------------------------------------------------------------------------
enum { SCREEN_PLANES = 0, SCREEN_METEO = 1, SCREEN_SETTINGS = 2, SCREEN_COUNT = 3 };
static int  s_screen = SCREEN_PLANES;
static bool s_forceDraw = false;

// --- Tecky prepinani obrazovek (dole uprostred) ---
//
// Krome dlouheho stisku jde na obrazovku skocit i primo klepnutim na tecku.
// Dlouhy stisk je rychly, ale neni videt - tecky ukazuji, kde uzivatel je,
// a zaroven slouzi jako tlacitka. Zona je na vsech obrazovkach volna:
// ScreenPlanes ji ma v inReservedZone(), meteoradar i Nastaveni tam nic nemaji.
#define DOTS_CY      308
#define DOTS_GAP      24
#define DOTS_R         6
#define DOTS_CX       (LCD_WIDTH / 2)
#define DOTS_HIT_HALF (DOTS_GAP * SCREEN_COUNT / 2)   // 36 px na kazdou stranu
#define DOTS_HIT_Y0   (DOTS_CY - 14)
#define DOTS_HIT_Y1   (DOTS_CY + 12)

static void drawScreenDots() {
  int startX = DOTS_CX - (SCREEN_COUNT - 1) * DOTS_GAP / 2;
  // Podklad, aby byly tecky citelne i nad mapou nebo srazkami.
  gfx->fillRoundRect(DOTS_CX - DOTS_HIT_HALF, DOTS_HIT_Y0,
                     DOTS_HIT_HALF * 2, DOTS_HIT_Y1 - DOTS_HIT_Y0, 6, C_BLACK);
  for (int i = 0; i < SCREEN_COUNT; i++) {
    int x = startX + i * DOTS_GAP;
    if (i == s_screen) gfx->fillCircle(x, DOTS_CY, DOTS_R, C_WHITE);
    else               gfx->drawCircle(x, DOTS_CY, DOTS_R, C_GRAY);
  }
}

// Vraci index obrazovky, na kterou se klepnulo, jinak -1.
static int dotsHitTest(int x, int y) {
  if (y < DOTS_HIT_Y0 || y > DOTS_HIT_Y1) return -1;
  if (x < DOTS_CX - DOTS_HIT_HALF || x > DOTS_CX + DOTS_HIT_HALF) return -1;
  int startX = DOTS_CX - (SCREEN_COUNT - 1) * DOTS_GAP / 2;
  int best = -1, bestD = DOTS_GAP / 2 + 2;
  for (int i = 0; i < SCREEN_COUNT; i++) {
    int d = abs(x - (startX + i * DOTS_GAP));
    if (d < bestD) { bestD = d; best = i; }
  }
  return best;
}

static void drawActive() {
  switch (s_screen) {
    case SCREEN_PLANES:   ScreenPlanes_Draw();   break;
    case SCREEN_METEO:    ScreenWeather_Draw();  break;
    case SCREEN_SETTINGS: ScreenSettings_Draw(); break;
  }
  drawScreenDots();
  gfx->flush();
}

static void activeEnter() {
  switch (s_screen) {
    case SCREEN_PLANES:   ScreenPlanes_Enter();   break;
    case SCREEN_METEO:    ScreenWeather_Enter();  break;
    case SCREEN_SETTINGS: ScreenSettings_Enter(); break;
  }
}

static bool activeTick() {
  switch (s_screen) {
    case SCREEN_PLANES:   return ScreenPlanes_Tick();
    case SCREEN_METEO:    return ScreenWeather_Tick();
    case SCREEN_SETTINGS: return ScreenSettings_Tick();
  }
  return false;
}

static void activeChangeRange(int dir) {
  switch (s_screen) {
    case SCREEN_PLANES: ScreenPlanes_ChangeRange(dir);  break;
    case SCREEN_METEO:  ScreenWeather_ChangeRange(dir); break;
    default: break;   // Nastaveni zadny rozsah nema
  }
}

static bool activeTap(int x, int y) {
  switch (s_screen) {
    case SCREEN_PLANES:   return ScreenPlanes_HandleTap(x, y);
    case SCREEN_SETTINGS: return ScreenSettings_HandleTap(x, y);
    default: return false;   // meteoradar nema dotykove cile
  }
}

static void gotoScreen(int idx) {
  if (idx < 0 || idx >= SCREEN_COUNT || idx == s_screen) return;
  s_screen = idx;
  Settings_SetScreen((uint8_t)s_screen);   // zapamatovat (zapis je odlozeny)
  Serial.printf("Obrazovka: %d\n", s_screen);
  activeEnter();
  s_forceDraw = true;
}

// Dlouhy stisk prepina smerove: dir -1 = predchozi, +1 = nasledujici, dokola.
static void switchScreen(int dir) {
  gotoScreen((s_screen + dir + SCREEN_COUNT) % SCREEN_COUNT);
}

// Tovarni reset. ESPD-3.5 nema tlacitko, ktere by slo drzet pri startu, takze
// se vyvolava z obrazovky Nastaveni (a ta si vyzada potvrzeni druhym klepnutim).
static void doFactoryReset() {
  gfx->fillScreen(C_BLACK);
  UI_TextCentered("Mazu nastaveni", LCD_HEIGHT / 2 - 10, C_RED, 2);
  UI_TextCentered("Deska se restartuje...", LCD_HEIGHT / 2 + 20, C_GRAY, 1);
  gfx->flush();
  Settings_ClearAll();
  WiFi_Reset();
  delay(1200);
  ESP.restart();
}

#if TOUCH_ENABLE
// ---------------------------------------------------------------------------
//  Dotyk: swipe (rozsah) vs. kratke klepnuti (vyber) vs. dlouhy stisk (obrazovka)
// ---------------------------------------------------------------------------
static void handleTouch() {
  static bool touching = false;
  static int  sx = 0, sy = 0, lx = 0, ly = 0;
  static unsigned long t0 = 0;

  int x, y;
  bool down = Touch_Read(&x, &y);
  if (down) {
    if (!touching) { touching = true; sx = x; sy = y; t0 = millis(); }
    lx = x; ly = y;
    return;
  }
  if (!touching) return;

  touching = false;
  int dx = lx - sx, dy = ly - sy;
  unsigned long dur = millis() - t0;
  bool smallMove = (abs(dx) < 30 && abs(dy) < 30);
  bool swipe     = (abs(dx) >= 60 && abs(dy) <= 50 && dur <= 700);

#if TOUCH_DEBUG
  Serial.printf("TOUCH: start=(%d,%d) konec=(%d,%d) dx=%d dy=%d %lums -> %s\n",
                sx, sy, lx, ly, dx, dy, (unsigned long)dur,
                swipe ? "swipe" : (smallMove && dur >= 500) ? "dlouhy stisk"
                      : smallMove ? "klepnuti" : "ignorovano");
#endif

  if (swipe) {
    activeChangeRange(dx < 0 ? +1 : -1);
    s_forceDraw = true;
    return;
  }
  if (!smallMove) return;

  // Tecky prepinani obrazovek maji prednost pred vsim ostatnim - jsou to
  // jedine "globalni" tlacitko a jejich zona je na vsech obrazovkach volna.
  int dot = dotsHitTest(lx, ly);
  if (dot >= 0) { gotoScreen(dot); return; }

  if (dur >= 500) {
    // DLOUHY stisk. Na obrazovce Nastaveni ale nesmi "prestrelit" tlacitko:
    // kdo drzi prst na "Firmware update" pomaleji nez pul sekundy, chce to
    // tlacitko, ne jinou obrazovku.
    if (s_screen == SCREEN_SETTINGS && ScreenSettings_HitsControl(lx, ly)) {
      if (activeTap(lx, ly)) s_forceDraw = true;
      return;
    }
    switchScreen(lx < LCD_WIDTH / 2 ? -1 : +1);
    return;
  }

  // KRATKE klepnuti -> obrazovka si ho zpracuje sama.
  if (activeTap(lx, ly)) s_forceDraw = true;
}
#endif

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n=== ESPD35_MeteoPlaneRadar v%s ===\n", FW_VERSION);

  Settings_Begin();

  // Displej + canvas.
  if (!gfx->begin(TFT_SPI_HZ)) {
    Serial.println("gfx->begin() SELHAL - zkontrolujte piny a OPI PSRAM");
  }
  gfx->setTextWrap(false);
  Backlight_Set(Settings_Backlight());
  gfx->fillScreen(C_BLACK);
  gfx->flush();

#if TOUCH_ENABLE
  Touch_Init();
#endif

  ADSB_SetPollFn(netPoll);
  CHMU_SetPollFn(netPoll);

  WiFi_ConnectOrPortal();
  if (WiFi_IsConnected()) {
    Serial.printf("WiFi: %s  IP: %s\n", WiFi_SSID().c_str(), WiFi_IP().c_str());
    configTzTime(TZ_INFO, NTP_SERVER);
    GeoIP_DetectIfNeeded();   // doplni polohu dle IP, kdyz ji uzivatel nezadal
  }

  // Obnovit posledni obrazovku (Enter() si obnovi i rozsah).
  s_screen = Settings_Screen();
  if (s_screen >= SCREEN_COUNT) s_screen = SCREEN_PLANES;
  activeEnter();
  drawActive();

  Watchdog_Begin();
  Serial.println("Setup hotov");
}

void loop() {
#if TOUCH_ENABLE
  handleTouch();
#endif
  WiFi_Loop();

  // Tovarni reset z obrazovky Nastaveni (uz potvrzeny druhym klepnutim).
  // Konci restartem, takze se za nej nedostaneme.
  if (ScreenSettings_WantsReset()) {
    ScreenSettings_ClearReset();
    doFactoryReset();
  }

  // Zadost o WiFi/AP portal z obrazovky Nastaveni (blokujici).
  if (ScreenSettings_WantsPortal()) {
    ScreenSettings_ClearPortal();
    Watchdog_Suspend();                 // portal blokuje - watchdog nema kdo krmit
    WiFi_StartPortal();                 // umozni zadat WiFi i polohu (lat/lon)
    Watchdog_Resume();
    if (WiFi_IsConnected()) configTzTime(TZ_INFO, NTP_SERVER);
    activeEnter();
    drawActive();
  }

  // Aktualizace firmwaru pres WiFi. OTA_Run() je blokujici a konci restartem,
  // takze se za nej za normalnich okolnosti nedostaneme.
  if (ScreenSettings_WantsOTA()) {
    ScreenSettings_ClearOTA();
    OTA_Run();
    activeEnter();
    drawActive();
  }

  static unsigned long lastDraw = 0;
  bool wantDraw = activeTick();

  if (s_forceDraw || (wantDraw && millis() - lastDraw >= 80)) {
    drawActive();
    lastDraw = millis();
    s_forceDraw = false;
  }

  Settings_Tick();   // odlozeny zapis stavu UI do NVS
  Watchdog_Feed();
  delay(5);
}
