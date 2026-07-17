// =============================================================================
//  ESPD35_MeteoPlaneRadar - radar letadel (adsb.fi) na LaskaKit ESPD-3.5"
// =============================================================================
//
//  Deska:   LaskaKit ESPD-3.5" (ESP32-S3), displej ILI9488 480x320 (SPI)
//
//  Dve obrazovky, prepinaji se dlouhym stiskem prstu (kdekoli na displeji):
//    1) LETADLA  - 3/4 mapa vlevo + 1/4 detail nejblizsiho letadla vpravo
//    2) METEO    - celoobrazovkovy srazkovy radar CHMU (Web Mercator vyrez)
//
//  Ovladani:
//    - dlouhy stisk prstu (>0,5 s) = prepnout LETADLA <-> METEO
//    - swipe vlevo/vpravo (dotyk)= zmena rozsahu aktivni obrazovky
//    - klepnuti na letadlo       = detail; klepnuti do prazdna = auto-nejblizsi
//    - kratky stisk tlacitka     = zmena rozsahu (aktivni obrazovka)
//    - dlouhy stisk tlacitka     = prepnuti jednotek (jen na obrazovce letadel)
//    - drzeni tlacitka WiFi = prepnuti do nastaveni (spusteni AP)
//
//  Port projektu petus/Meteo-PlaneRadar (kulaty displej Waveshare 480x480).
//  Zachovana izotropni projekce -> spravne meritko a shoda dat s mesty
//  (letadla plocha azimutalni projekce, meteo Web Mercator).
//
//  Knihovny (Arduino IDE, ESP32 core 3.x):
//    - GFX Library for Arduino (moononournation)  - kresleni + canvas
//    - PNGdec (bitbank2)                          - dekodovani snimku CHMU
//    - ArduinoJson (bblanchon, v7)                - parsovani ADS-B dat
//    - WiFiManager (tzapu)                         - konfiguracni WiFi portal
//    - QRCode (ricmoo)                             - QR kod v AP portalu
//
//  Zdroje dat (nutno uvest, jen pro osobni nekomercni pouziti):
//    - Letadla: adsb.fi, https://adsb.fi
//    - Srazky:  Cesky hydrometeorologicky ustav (CHMU), https://opendata.chmi.cz
//    - Poloha:  ip-api.com (automaticka detekce dle IP)
//
//  Licence: MIT. Na displeji se zobrazuje napis "laskakit.cz".
//  Puvodni projekt petus/Meteo-PlaneRadar vznikl pro chiptron.cz (atribuce).
// =============================================================================

#include <Arduino_GFX_Library.h>
#include <time.h>
#include "esp_arduino_version.h"

#include "Config.h"
#include "UI.h"
#include "Settings.h"
#include "WiFiPortal.h"
#include "GeoIP.h"
#include "ADSB.h"
#include "ScreenPlanes.h"
#include "CHMU.h"
#include "ScreenWeather.h"
#include "Watchdog.h"
#if TOUCH_ENABLE
  #include "Touch_FT6236.h"
#endif

// --- Displej: ILI9488 (SPI) + off-screen canvas v PSRAM (proti blikani) ---
Arduino_DataBus* bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO);
Arduino_GFX*     panel = new Arduino_ILI9488_18bit(bus, TFT_RST, LCD_ROTATION, false /*IPS*/);

// gfx = canvas. Vsechno kresleni jde sem, flush() posle cely snimek najednou.
Arduino_GFX* gfx = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, panel);

static void netPoll() { yield(); }

// ---------------------------------------------------------------------------
//  Podsviceni (PWM), kompatibilni s core 2.x i 3.x.
// ---------------------------------------------------------------------------
static void Backlight_Set(uint8_t pct) {
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
//  Tlacitko: kratky vs. dlouhy stisk.
// ---------------------------------------------------------------------------
static bool buttonDown() {
  int v = digitalRead(BUTTON_PIN);
  return BUTTON_ACTIVE_LOW ? (v == LOW) : (v == HIGH);
}

static bool s_forceDraw = false;

// --- Spravce obrazovek: 0 = letadla, 1 = meteoradar ---
enum { SCREEN_PLANES = 0, SCREEN_METEO = 1 };
static int s_screen = SCREEN_PLANES;

// Smerovani na aktivni obrazovku.
static void activeChangeRange(int dir) {
  if (s_screen == SCREEN_PLANES) ScreenPlanes_ChangeRange(dir);
  else                           ScreenWeather_ChangeRange(dir);
}
static bool activeTick() {
  return (s_screen == SCREEN_PLANES) ? ScreenPlanes_Tick() : ScreenWeather_Tick();
}
static void activeEnter() {
  if (s_screen == SCREEN_PLANES) ScreenPlanes_Enter();
  else                           ScreenWeather_Enter();
}

static void handleButton() {
  static bool prev = false;
  static unsigned long tDown = 0;
  bool down = buttonDown();
  if (down && !prev) { prev = true; tDown = millis(); }
  else if (!down && prev) {
    prev = false;
    unsigned long dur = millis() - tDown;
    if (dur >= 40 && dur < 800) {           // kratky stisk -> rozsah (aktivni obrazovka)
      activeChangeRange(+1);
      s_forceDraw = true;
    } else if (dur >= 800) {                 // dlouhy stisk -> jednotky (jen letadla)
      if (s_screen == SCREEN_PLANES) ScreenPlanes_ToggleUnits();
      s_forceDraw = true;
    }
  }
}

#if TOUCH_ENABLE
// Dotyk: rozliseni gest - vodorovny swipe (zmena rozsahu) vs. tap (vyber).
static void handleTouch() {
  static bool touching = false;
  static int  sx = 0, sy = 0, lx = 0, ly = 0;
  static unsigned long t0 = 0;

  int x, y;
  bool down = Touch_Read(&x, &y);
  if (down) {
    if (!touching) { touching = true; sx = x; sy = y; t0 = millis(); }
    lx = x; ly = y;
  } else if (touching) {
    touching = false;
    int dx = lx - sx, dy = ly - sy;
    unsigned long dur = millis() - t0;
    if (abs(dx) >= 60 && abs(dy) <= 50 && dur <= 700) {
      // Swipe = zmena rozsahu na aktivni obrazovce.
      activeChangeRange(dx < 0 ? +1 : -1);
      s_forceDraw = true;
    } else if (abs(dx) < 30 && abs(dy) < 30) {   // stisk na jednom miste
      if (dur >= 500) {                          // DLOUHY stisk kdekoli -> prepnout obrazovku
        s_screen = (s_screen == SCREEN_PLANES) ? SCREEN_METEO : SCREEN_PLANES;
        activeEnter();
        s_forceDraw = true;
      } else if (s_screen == SCREEN_PLANES) {    // kratky tap -> vyber letadla / tlacitka
        if (ScreenPlanes_HandleTap(lx, ly)) s_forceDraw = true;
      }
    }
  }
}
#endif

// Drzeni tlacitka pri startu -> reset nastaveni.
static void checkBootReset() {
  if (!buttonDown()) return;
  gfx->fillScreen(C_BLACK);
  UI_TextCentered("Drzte pro reset...", LCD_HEIGHT / 2, C_WHITE, 2);
  gfx->flush();
  unsigned long start = millis();
  while (buttonDown()) {
    if (millis() - start >= 3000) {
      UI_TextCentered("Mazu nastaveni", LCD_HEIGHT / 2 + 30, C_RED, 2);
      gfx->flush();
      Settings_ClearAll();
      WiFi_Reset();
      delay(800);
      ESP.restart();
    }
    delay(20);
  }
}

static void drawActive() {
  if (s_screen == SCREEN_PLANES) ScreenPlanes_Draw();
  else                           ScreenWeather_Draw();
  gfx->flush();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== ESPD35 PlaneRadar ===");

  Settings_Begin();

  pinMode(BUTTON_PIN, BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);

  // Displej + canvas.
  if (!gfx->begin(TFT_SPI_HZ)) {
    Serial.println("gfx->begin() SELHAL - zkontrolujte piny a OPI PSRAM");
  }
  Backlight_Set(Settings_Backlight());
  gfx->fillScreen(C_BLACK);
  gfx->flush();

  checkBootReset();

#if TOUCH_ENABLE
  Touch_Init();
#endif

  ADSB_SetPollFn(netPoll);
  CHMU_SetPollFn(netPoll);

  WiFi_ConnectOrPortal();
  if (WiFi_IsConnected()) {
    Serial.printf("WiFi: %s  IP: %s\n", WiFi_SSID().c_str(), WiFi_IP().c_str());
    configTzTime(TZ_INFO, "pool.ntp.org");
    GeoIP_DetectIfNeeded();   // doplni polohu dle IP, kdyz ji uzivatel nezadal
  }

  s_screen = SCREEN_PLANES;
  activeEnter();
  drawActive();

  Watchdog_Begin();
}

void loop() {
  handleButton();
#if TOUCH_ENABLE
  handleTouch();
#endif
  WiFi_Loop();

  // Zadost o WiFi/AP portal z tlacitka v panelu (blokujici - kresli AP obrazovku).
  if (ScreenPlanes_WantsPortal()) {
    ScreenPlanes_ClearPortal();
    Watchdog_Suspend();
    WiFi_StartPortal();                 // umozni zadat WiFi i polohu (lat/lon)
    Watchdog_Resume();
    if (WiFi_IsConnected()) configTzTime(TZ_INFO, "pool.ntp.org");
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

  Watchdog_Feed();
  delay(5);
}
