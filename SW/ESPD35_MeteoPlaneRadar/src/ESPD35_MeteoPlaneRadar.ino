// =============================================================================
//  ESPD35_MeteoPlaneRadar
//  Hodiny, radar letadel (adsb.fi), srazkovy meteoradar CHMU a predpoved
//  pocasi (Open-Meteo) na desce LaskaKit ESPD-3.5".
// =============================================================================
//
//  Deska:   LaskaKit ESPD-3.5" (ESP32-S3, 16 MB flash), ILI9488 480x320 (SPI),
//           kapacitni dotyk FT5436 (I2C, knihovna FT6236)
//
//  PET OBRAZOVEK (ctyri z nich se daji vypnout ve webovem rozhrani):
//    1) HODINY     - cas, datum, aktualni pocasi, beh sekund po obvodu
//    2) LETADLA    - 3/4 mapa vlevo + 1/4 detail letadla vpravo
//    3) METEORADAR - celoobrazovkovy srazkovy radar CHMU s animaci
//    4) PREDPOVED  - nejblizsi hodiny a dny, ovzdusi a pyl
//    5) NASTAVENI  - jas, orientace mapy, jednotky, adresa webu (nejde vypnout)
//
//  Konfigurace je V PROHLIZECI. Deska trvale obsluhuje vlastni stranku na
//  http://espd35meteoradar.local/ , jakmile je na domaci siti, a captive portal
//  na vlastnim pristupovem bode, dokud neni. Aktualizace firmwaru je na /update
//  tehoz serveru.
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
//    - ArduinoJson (bblanchon, v7)                - vsechen JSON
//    - QRCode (ricmoo)                            - QR kod v portalu (prilozeno)
//    - FT6236 (prilozena LaskaKit)                - kapacitni dotyk
//    - WebServer, DNSServer, ESPmDNS, Preferences, HTTPClient, Update
//                                                 - soucast ESP32 core
//
//  WiFiManager ani ElegantOTA se uz NEPOUZIVAJI - viz WiFiPortal.h a WebConfig.h.
//
//  POZOR: pro OTA je nutna vlastni tabulka oddilu (partitions.csv, dva
//  aplikacni sloty po 6 MB) - v Arduino IDE zvolte Partition Scheme = Custom.
//
//  Zdroje dat (nutno uvest, jen pro osobni nekomercni pouziti):
//    - Letadla:  adsb.fi, https://adsb.fi
//    - Trasa:    adsb.lol, https://adsb.lol
//                (data tras: https://github.com/vradarserver/standing-data)
//    - Srazky:   Cesky hydrometeorologicky ustav, https://opendata.chmi.cz
//    - Pocasi:   Open-Meteo, https://open-meteo.com
//    - Poloha:   ip-api.com (automaticka detekce dle IP)
//    - Mapa:     Natural Earth (volne dilo), GeoNames (CC BY 4.0)
//
//  Licence: MIT. Na displeji se zobrazuje napis "laskakit.cz".
//  Puvodni projekt petus/MeteoPlaneRadar vznikl pro chiptron.cz (atribuce).
//
//  Verze: Version.h (FW_VERSION).  Historie zmen: CHANGELOG.md.
// =============================================================================

#include <Arduino_GFX_Library.h>
#include <time.h>
#include "esp_arduino_version.h"
#include "esp_system.h"

#include "Config.h"
#include "Version.h"
#include "UI.h"
#include "Layout.h"
#include "Lang.h"
#include "Settings.h"
#include "Net.h"
#include "Clock.h"
#include "Status.h"
#include "WiFiPortal.h"
#include "WebConfig.h"
#include "GeoIP.h"
#include "ADSB.h"
#include "Route.h"
#include "ScreenPlanes.h"
#include "CHMU.h"
#include "ScreenWeather.h"
#include "ScreenClock.h"
#include "ScreenForecast.h"
#include "ScreenSettings.h"
#include "Forecast.h"
#include "Outside.h"
#include "NightMode.h"
#include "Watchdog.h"
#if TOUCH_ENABLE
  #include "Touch_FT6236.h"
#endif

// --- Displej: ILI9488 (SPI) + off-screen canvas v PSRAM (proti blikani) ---
Arduino_DataBus* bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO);
Arduino_GFX*     panel = new Arduino_ILI9488_18bit(bus, TFT_RST, LCD_ROTATION, false /*IPS*/);

// gfx = canvas. Vsechno kresleni jde sem, flush() posle cely snimek najednou.
Arduino_GFX* gfx = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, panel);

#if TOUCH_ENABLE
static void touchPump();      // definice nize; netPoll ji vola driv
#endif

// Vola se opakovane behem dlouhych prenosu:
//   - yield() + nakrmit watchdog, aby stahovani ADS-B (bufferovane cteni
//     + jeden retry) a sesti snimku meteoradaru nemohlo trefit WDT,
//   - a nove take SNIMAT DOTYK.
//
// To posledni je to podstatne. Do 0.3.0 se dotyk cetl jen na zacatku loop(),
// jenze stahovani animace CHMU trva sekundy - klepnuti behem nej se proste
// ztratilo. touchPump() nic nekresli a nesituje, takze je bezpecne ho volat
// odsud; gesto se tim rozpozna ve chvili, kdy vznikne, a PROVEDE se az
// v loop(), kde nic jineho nekresli.
static void netPoll() {
  yield();
  Watchdog_Feed();
#if TOUCH_ENABLE
  touchPump();
#endif
}

// ---------------------------------------------------------------------------
//  Podsviceni (PWM), kompatibilni s core 2.x i 3.x.
//  Neni static - vola ho i obrazovka Nastaveni (posuvnik jasu) a NightMode.
//  Deklarace je v UI.h.
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

// ===========================================================================
//  Sprava obrazovek
//
//  Pet obrazovek, ctyri z nich vypinatelne. "Vypnuta" znamena OPRAVDU
//  neviditelna: dlouhy stisk ji preskoci, automaticke stridani ji preskoci
//  a nema dole tecku - takze tecky vzdy odpovidaji tomu, co dlouhy stisk
//  skutecne udela. Nastaveni vypnout nejde, jinak by deska s vypnutym zbytkem
//  nemela cestu zpatky k webovemu rozhrani.
// ===========================================================================
static int  s_screen = SCREEN_PLANES_I;
static bool s_forceDraw = false;

static bool screenVisible(int i) { return Settings_ScreenEnabled((uint8_t)i); }

// Viditelne obrazovky v poradi - pro tecky i pro krokovani.
static int visibleList(int* out) {
  int n = 0;
  for (int i = 0; i < SCREEN_N; i++) if (screenVisible(i)) out[n++] = i;
  return n;
}

// --- Tecky prepinani obrazovek (dole uprostred) ---
//
// Krome dlouheho stisku jde na obrazovku skocit i primo klepnutim na tecku.
// Dlouhy stisk je rychly, ale neni videt - tecky ukazuji, kde uzivatel je,
// a zaroven slouzi jako tlacitka. Zona je na vsech obrazovkach volna: kazda
// si ji rezervuje pres Layout, takze se pod ni nic nenakresli.
static void drawScreenDots() {
  int vis[SCREEN_N];
  const int n = visibleList(vis);
  if (n <= 1) return;                  // jedina tecka nic nerika

  const int gap = LY_DOTS_GAP;
  const int cx  = LCD_WIDTH / 2;
  const int startX = cx - (n - 1) * gap / 2;
  const int halfW  = gap * n / 2;

  // Podklad, aby byly tecky citelne i nad mapou nebo srazkami.
  gfx->fillRoundRect(cx - halfW, LY_DOTS_Y0, halfW * 2, LY_DOTS_H, 6, C_BLACK);
  for (int i = 0; i < n; i++) {
    int x = startX + i * gap;
    if (vis[i] == s_screen) gfx->fillCircle(x, LY_DOTS, LY_DOTS_R, C_WHITE);
    else                    gfx->drawCircle(x, LY_DOTS, LY_DOTS_R, C_GRAY);
  }
}

// Vraci index obrazovky, na kterou se klepnulo, jinak -1.
static int dotsHitTest(int x, int y) {
  int vis[SCREEN_N];
  const int n = visibleList(vis);
  if (n <= 1) return -1;

  const int gap = LY_DOTS_GAP;
  const int cx  = LCD_WIDTH / 2;
  const int halfW = gap * n / 2;
  if (y < LY_DOTS_Y0 || y > LY_DOTS_Y0 + LY_DOTS_H) return -1;
  if (x < cx - halfW || x > cx + halfW) return -1;

  const int startX = cx - (n - 1) * gap / 2;
  int best = -1, bestD = gap / 2 + 2;
  for (int i = 0; i < n; i++) {
    int d = abs(x - (startX + i * gap));
    if (d < bestD) { bestD = d; best = vis[i]; }
  }
  return best;
}

static void drawActive() {
  switch (s_screen) {
    case SCREEN_CLOCK_I:    ScreenClock_Draw();    break;
    case SCREEN_PLANES_I:   ScreenPlanes_Draw();   break;
    case SCREEN_METEO_I:    ScreenWeather_Draw();  break;
    case SCREEN_FORECAST_I: ScreenForecast_Draw(); break;
    case SCREEN_SETTINGS_I: ScreenSettings_Draw(); break;
  }
  drawScreenDots();
  gfx->flush();
}

static void activeEnter() {
  switch (s_screen) {
    case SCREEN_CLOCK_I:    ScreenClock_Enter();    break;
    case SCREEN_PLANES_I:   ScreenPlanes_Enter();   break;
    case SCREEN_METEO_I:    ScreenWeather_Enter();  break;
    case SCREEN_FORECAST_I: ScreenForecast_Enter(); break;
    case SCREEN_SETTINGS_I: ScreenSettings_Enter(); break;
  }
}

static bool activeTick() {
  switch (s_screen) {
    case SCREEN_CLOCK_I:    return ScreenClock_Tick();
    case SCREEN_PLANES_I:   return ScreenPlanes_Tick();
    case SCREEN_METEO_I:    return ScreenWeather_Tick();
    case SCREEN_FORECAST_I: return ScreenForecast_Tick();
    case SCREEN_SETTINGS_I: return ScreenSettings_Tick();
  }
  return false;
}

static void activeChangeRange(int dir) {
  switch (s_screen) {
    case SCREEN_PLANES_I: ScreenPlanes_ChangeRange(dir);  break;
    case SCREEN_METEO_I:  ScreenWeather_ChangeRange(dir); break;
    default: break;   // ostatni obrazovky zadny rozsah nemaji
  }
}

static bool activeTap(int x, int y) {
  switch (s_screen) {
    case SCREEN_CLOCK_I:    return ScreenClock_HandleTap(x, y);
    case SCREEN_PLANES_I:   return ScreenPlanes_HandleTap(x, y);
    case SCREEN_SETTINGS_I: return ScreenSettings_HandleTap(x, y);
    default: return false;   // meteoradar ani predpoved nemaji dotykove cile
  }
}

static void gotoScreen(int idx) {
  if (idx < 0 || idx >= SCREEN_N || idx == s_screen) return;
  if (!screenVisible(idx)) return;
  s_screen = idx;
  Settings_SetScreen((uint8_t)s_screen);   // zapamatovat (zapis je odlozeny)
  Serial.printf("Obrazovka: %d\n", s_screen);
  activeEnter();
  s_forceDraw = true;
}

// Dlouhy stisk prepina smerove: dir -1 = predchozi, +1 = nasledujici, dokola.
// Vypnute obrazovky se preskakuji; pojistka zabrani nekonecne smycce, kdyby
// nakrasne nebyla zapnuta ani jedna.
static void switchScreen(int dir) {
  int next = s_screen;
  for (int guard = 0; guard < SCREEN_N; guard++) {
    next = (next + dir + SCREEN_N) % SCREEN_N;
    if (screenVisible(next)) break;
  }
  gotoScreen(next);
}

// Tovarni reset. ESPD-3.5 nema tlacitko, ktere by slo drzet pri startu, takze
// se vyvolava z obrazovky Nastaveni (a ta si vyzada potvrzeni druhym klepnutim)
// nebo z webove stranky.
static void doFactoryReset() {
  gfx->fillScreen(C_BLACK);
  UI_TextCentered("Mazu nastaveni", LCD_HEIGHT / 2 - 10, C_RED, 2);
  UI_TextCentered("Deska se restartuje...", LCD_HEIGHT / 2 + 20, C_GRAY, 1);
  gfx->flush();
  Settings_ClearAll();
  delay(1200);
  ESP.restart();
}

#if TOUCH_ENABLE
// ---------------------------------------------------------------------------
//  Dotyk: snimani a provedeni, zamerne oddelene
//
//  Stahovani blokuje uvnitr activeTick() na nekolik sekund a dotyk se do
//  0.3.0 cetl jen na zacatku loop() - klepnuti behem stahovani se tedy
//  zahodilo, nebo se sejmulo tak pozde, ze vyslo jako jine gesto.
//  Obsluha je proto rozdelena:
//
//    SNIMANI (touchPump) cte radic a posouva gesto. Nic nekresli a nic
//    nestahuje, takze je bezpecne ho volat odkudkoli - vcetne netPoll(),
//    ktery bezi po celou dobu prenosu. Gesto se rozpozna, kdyz vznikne.
//
//    PROVEDENI (dispatchTouch) na nej reaguje, a to jen z loop().
//    Prekreslovat zevnitr stahovani by znamenalo vlezt do kresliciho kodu
//    uprostred snimku.
// ---------------------------------------------------------------------------
enum PendKind : uint8_t { PEND_NONE = 0, PEND_SWIPE, PEND_LONG, PEND_TAP };
static PendKind s_pendKind = PEND_NONE;
static int      s_pendA = 0, s_pendB = 0;

static bool          s_touching = false;
static int           s_startX = 0, s_startY = 0;
static int           s_lastX = 0, s_lastY = 0;
static unsigned long s_startMs = 0;
static unsigned long s_lastSeenMs = 0;
static unsigned long s_lastPump = 0;

// Gesto NEKONCI na prvnim prazdnem vzorku. FT5436 obcas jeden vynecha
// a Touch_Read() dalsi zahazuje ve svych filtrech - prazdny vzorek uprostred
// tazeni je normalni jev, ne zvednuty prst. Ukoncit tam gesto znamenalo
// rozpadnout jeden swipe na nekolik klepnuti do mapy.
static void touchPump() {
  unsigned long now = millis();
  if (now - s_lastPump < TOUCH_PUMP_MS) return;
  s_lastPump = now;

  int x, y;
  if (Touch_Read(&x, &y)) {
    if (!s_touching) { s_touching = true; s_startX = x; s_startY = y; s_startMs = now; }
    s_lastX = x; s_lastY = y;
    s_lastSeenMs = now;
    return;
  }

  if (!s_touching) return;
  if (now - s_lastSeenMs < TOUCH_RELEASE_MS) return;   // jeste to nemusi byt konec

  s_touching = false;
  const int dx = s_lastX - s_startX;
  const int dy = s_lastY - s_startY;
  // Delka gesta se meri k POSLEDNIMU VIDENEMU vzorku, ne k millis() - jinak by
  // se do ni pricetlo tech 60 ms ticha a kratke klepnuti by se blizilo hranici
  // dlouheho stisku.
  const unsigned long dur = s_lastSeenMs - s_startMs;
  const bool smallMove = (abs(dx) < 30 && abs(dy) < 30);
  const bool swipe     = (abs(dx) >= 60 && abs(dy) <= 50 && dur <= 700);

#if TOUCH_DEBUG
  Serial.printf("TOUCH: start=(%d,%d) konec=(%d,%d) dx=%d dy=%d %lums -> %s\n",
                s_startX, s_startY, s_lastX, s_lastY, dx, dy, (unsigned long)dur,
                swipe ? "swipe" : (smallMove && dur >= 500) ? "dlouhy stisk"
                      : smallMove ? "klepnuti" : "ignorovano");
#endif

  // Nejnovejsi gesto vyhrava. Kdyz jedno jeste ceka za dlouhym stahovanim,
  // uzivatel si to mezitim rozmyslel - a provest to stare by byla spatna
  // odpoved, ktera dorazi pozde.
  if (swipe) {
    s_pendKind = PEND_SWIPE;
    s_pendA = (dx < 0) ? +1 : -1;
  } else if (smallMove && dur >= 500) {
    s_pendKind = PEND_LONG;
    s_pendA = s_lastX; s_pendB = s_lastY;
  } else if (smallMove) {
    s_pendKind = PEND_TAP;
    s_pendA = s_lastX; s_pendB = s_lastY;
  }
}
#endif

// --- Automaticke stridani obrazovek ----------------------------------------
// Pozastavi ho gesto, ktere znamena "ridim to sam" - swipe nebo dlouhy stisk -
// a drzi ho otevreny detail letadla. Obycejne klepnuti ho nezastavuje: byt
// prepnut uprostred cteni je otravne, ale stejne tak zarizeni, ktere prestane
// stridat, protoze nekdo zavadil o sklo.
static unsigned long s_lastRotate = 0;
static unsigned long s_touchPauseUntil = 0;
static uint16_t      s_lastRotSec = 0xFFFF;   // pro detekci zmeny nastaveni

// Jak dlouho se stridani pozastavi po gestu.
//
// Puvodne to bylo pevnych AUTO_ROTATE_PAUSE_MS (10 minut) bez ohledu na
// interval. U tri minut to davalo smysl, u deseti sekund to znamenalo, ze po
// jedinem doteku uzivatel deset minut nic nevidi a mysli si, ze stridani
// nefunguje. Pauza je proto desetinasobek intervalu, nejmene 30 s a nejvys
// tech puvodnich 10 minut.
static unsigned long rotatePauseMs() {
  const unsigned long secs = Settings_AutoRotateSec();
  unsigned long p = secs * 10UL * 1000UL;
  if (p < 30000UL) p = 30000UL;
  if (p > AUTO_ROTATE_PAUSE_MS) p = AUTO_ROTATE_PAUSE_MS;
  return p;
}

static bool activeModalOpen() {
  return (s_screen == SCREEN_PLANES_I) && ScreenPlanes_DetailOpen();
}

static void autoRotateTick() {
  const uint16_t secs = Settings_AutoRotateSec();

  // Nove ulozena hodnota musi zabrat HNED. Bez tohohle se cekalo na doběhnutí
  // pauzy z posledniho doteku - uzivatel nastavil deset sekund, nic se
  // nedelo a vypadalo to jako rozbite.
  if (secs != s_lastRotSec) {
    s_lastRotSec = secs;
    s_lastRotate = millis();
    s_touchPauseUntil = 0;
    if (secs) Serial.printf("Stridani obrazovek: %u s\n", (unsigned)secs);
    else      Serial.println("Stridani obrazovek: vypnuto");
  }
  if (secs == 0) return;

  unsigned long now = millis();

  // Otevreny detail letadla stridani drzi: uzivatel ho cte. Casovac se pritom
  // udrzuje cerstvy, ne zastaveny - kdyz se panel zavre, zacne se pocitat cely
  // interval znovu misto okamziteho prepnuti.
  if (activeModalOpen()) { s_lastRotate = now; return; }

  if (now < s_touchPauseUntil) return;
  if (now - s_lastRotate < (unsigned long)secs * 1000UL) return;
  s_lastRotate = now;

  // Z Nastaveni se odchazi taky - drive tu byla tvrda vyjimka, takze deska
  // nechana na Nastaveni uz se sama nikdy nevratila k datum. Uzivatele, ktery
  // s Nastavenim prave pracuje, chrani pauza po doteku; ta je na to jediny
  // spravny nastroj.
  //
  // Krok na dalsi viditelnou DATOVOU obrazovku, Nastaveni se preskakuje.
  int next = s_screen;
  for (int guard = 0; guard < SCREEN_N; guard++) {
    next = (next + 1) % SCREEN_N;
    if (next != SCREEN_SETTINGS_I && screenVisible(next)) break;
  }
  if (next == s_screen || next == SCREEN_SETTINGS_I) return;
  Serial.printf("Stridani -> obrazovka %d\n", next);
  gotoScreen(next);
}

#if TOUCH_ENABLE
// Provede gesto sejmute driv (treba i behem stahovani). Vola se jen z loop().
static void dispatchTouch() {
  if (s_pendKind == PEND_NONE) return;
  const PendKind kind = s_pendKind;
  const int a = s_pendA, b = s_pendB;
  s_pendKind = PEND_NONE;

  if (kind == PEND_SWIPE) {
    // S otevrenym detailem swipe jen zavre panel, aby se omylem nezmenilo
    // meritko mapy za nim.
    if (activeModalOpen()) ScreenPlanes_CloseDetail();
    else                   activeChangeRange(a);
    s_forceDraw = true;
    s_touchPauseUntil = millis() + rotatePauseMs();
    return;
  }

  // Tecky prepinani obrazovek maji prednost pred vsim ostatnim - jsou to
  // jedine "globalni" tlacitko a jejich zona je na vsech obrazovkach volna.
  const int dot = dotsHitTest(a, b);
  if (dot >= 0) {
    gotoScreen(dot);
    s_touchPauseUntil = millis() + rotatePauseMs();
    return;
  }

  if (kind == PEND_LONG) {
    // Na obrazovce Nastaveni nesmi dlouhy stisk "prestrelit" tlacitko: kdo
    // drzi prst na tlacitku pomaleji nez pul sekundy, chce to tlacitko, ne
    // jinou obrazovku.
    if (s_screen == SCREEN_SETTINGS_I && ScreenSettings_HitsControl(a, b)) {
      if (activeTap(a, b)) s_forceDraw = true;
      return;
    }
    if (activeModalOpen()) { ScreenPlanes_CloseDetail(); s_forceDraw = true; }
    else                   switchScreen(a < LCD_WIDTH / 2 ? -1 : +1);
    s_touchPauseUntil = millis() + rotatePauseMs();
    return;
  }

  // KRATKE klepnuti -> obrazovka si ho zpracuje sama. Stridani nepozastavuje.
  if (activeTap(a, b)) s_forceDraw = true;
}
#endif

static const char* resetReasonText() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "zapnuti napajeni";
    case ESP_RST_EXT:       return "externi reset";
    case ESP_RST_SW:        return "softwarovy restart (napr. po OTA)";
    case ESP_RST_PANIC:     return "PANIC - vyjimka v programu";
    case ESP_RST_INT_WDT:   return "WATCHDOG (preruseni)";
    case ESP_RST_TASK_WDT:  return "WATCHDOG (zaseknuta smycka)";
    case ESP_RST_WDT:       return "WATCHDOG (jiny)";
    case ESP_RST_DEEPSLEEP: return "probuzeni z deep sleep";
    case ESP_RST_BROWNOUT:  return "BROWNOUT - podpeti napajeni";
    default:                return "neznamy";
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n=== ESPD35_MeteoPlaneRadar v%s ===\n", FW_VERSION);
  Serial.printf("Duvod restartu: %s\n", resetReasonText());

  Settings_Begin();
  Layout_SelfTest();         // nic nedela, dokud neni LAYOUT_DEBUG

  // Displej + canvas.
  if (!gfx->begin(TFT_SPI_HZ)) {
    Serial.println("gfx->begin() SELHAL - zkontrolujte piny a OPI PSRAM");
  }
  gfx->setTextWrap(false);
  NightMode_Apply();         // posle na panel spravnou uroven jasu
  gfx->fillScreen(C_BLACK);
  gfx->flush();

#if TOUCH_ENABLE
  Touch_Init();
#endif

  ADSB_SetPollFn(netPoll);
  CHMU_SetPollFn(netPoll);
  Net_SetPollFn(netPoll);

  // Pripoji se ulozenymi udaji, nebo vyvesi pristupovy bod a necha ho nahore.
  // Web server bezi, at to dopadne jakkoli.
  WiFi_Begin();

  if (WiFi_IsConnected()) {
    Serial.printf("WiFi: %s  IP: %s\n", WiFi_SSID().c_str(), WiFi_IP().c_str());
    // NTP zustava hlavnim zdrojem casu. Kdyby neprosel (nekteri poskytovatele
    // blokuji UDP port 123), nastavi se hodiny z hlavicky Date prvni odpovedi,
    // kterou stejne stahujeme - viz Clock.h.
    configTzTime(TZ_INFO, NTP_SERVER);
    GeoIP_DetectIfNeeded();   // doplni polohu dle IP, kdyz ji uzivatel nezadal
  }

  // Obnovit posledni obrazovku (Enter() si obnovi i rozsah).
  s_screen = Settings_Screen();
  if (s_screen >= SCREEN_N || !screenVisible(s_screen)) {
    // Ulozena obrazovka byla mezitim vypnuta - zacit na prvni zapnute.
    s_screen = SCREEN_SETTINGS_I;
    for (int i = 0; i < SCREEN_N; i++) if (screenVisible(i)) { s_screen = i; break; }
  }

  // V rezimu pristupoveho bodu patri displej QR kodu a musi mu patrit, dokud
  // nekdo nezada sit. Kreslit sem datovou obrazovku by znamenalo prekreslit
  // presne to, co uzivatel potrebuje videt. Obrazovka je ale uz vybrana, takze
  // je pripravena na okamzik, kdy se WiFi zvedne.
  if (!WiFi_IsAP()) { activeEnter(); drawActive(); }

  Watchdog_Begin();
  Serial.println("Setup hotov");
}

void loop() {
  // Nahravani firmwaru ma stroj pro sebe: zapis do flash a kresleni si nemaji
  // co prekazet a kazdy volny cyklus patri prenosu. Watchdog krmi obsluha
  // nahravani.
  if (WebConfig_UpdateBusy()) {
    WebConfig_Loop();
    delay(1);
    return;
  }

  // Dotyk: sejmout. Behem stahovani snimani probiha i uvnitr netPoll(),
  // takze nez se dostaneme sem, gesto uz ceka.
#if TOUCH_ENABLE
  touchPump();
#endif

  // --- Pristupovy bod: displej patri portalu -------------------------------
  // Dokud neni zadana sit, neni stejne co na obrazovky davat a QR kod je
  // jedine, co stoji mezi uzivatelem a funkcni deskou. Nic nize tedy nesmi
  // kreslit a zadne gesto nesmi prepnout pryc. Web server a stavovy automat
  // WiFi bezi dal.
  static bool s_apOwnsScreen = false;
  if (WiFi_IsAP()) {
#if TOUCH_ENABLE
    s_pendKind = PEND_NONE;            // zbloudily swipe nesmi vzit QR kod
#endif
    if (!s_apOwnsScreen) { s_apOwnsScreen = true; WiFi_DrawApScreen(); }
    WiFi_Loop();                       // muze prevzit udaje a opustit AP rezim
    WebConfig_Loop();
    if (WebConfig_WantsFactoryReset()) doFactoryReset();
    if (WebConfig_WantsRestart()) {
      Serial.println("Nastaveni zmeneno, restartuji");
      Serial.flush();
      delay(400);
      ESP.restart();
    }
    // Zadny Forecast_Tick - neni kudy na internet a kazdy pokus by jen
    // propalil smycku na timeoutech.
    NightMode_Tick();
    Settings_Tick();
    Watchdog_Feed();
    delay(5);
    return;
  }
  if (s_apOwnsScreen) {
    // Sit byla prijata: vzit si displej zpatky a rozjet hodiny stridani od
    // ted, aby prvni prepnuti prislo cely interval po konci nastavovani.
    s_apOwnsScreen = false;
    s_lastRotate = millis();
    s_touchPauseUntil = 0;
#if TOUCH_ENABLE
    s_pendKind = PEND_NONE;
#endif
    activeEnter();
    drawActive();
  }

#if TOUCH_ENABLE
  dispatchTouch();
#endif

  // Tytez akce, jen vyzadane z webu misto ze skla. Provadeji se TADY a ne
  // v obsluze pozadavku ze stejneho duvodu jako u gesta - kreslit se nesmi
  // odnikud jinud.
  {
    const int scr = WebConfig_TakeScreen();
    if (scr >= 0) {
      if (activeModalOpen()) ScreenPlanes_CloseDetail();
      gotoScreen(scr);
      s_touchPauseUntil = millis() + rotatePauseMs();
    }
    const int step = WebConfig_TakeScreenStep();
    if (step) {
      if (activeModalOpen()) ScreenPlanes_CloseDetail();
      switchScreen(step);
      s_touchPauseUntil = millis() + rotatePauseMs();
    }
    const int rng = WebConfig_TakeRangeStep();
    if (rng) {
      activeChangeRange(rng);
      s_forceDraw = true;
      s_touchPauseUntil = millis() + rotatePauseMs();
    }
  }

  WiFi_Loop();
  WebConfig_Loop();

  // --- Zmeny z webu, ktere se ODEHRAJI ZA BEHU (bez restartu) --------------
  //
  // Do 0.3.0 si zmena polohy i zmena sady obrazovek rikala o restart. Ani jedno
  // to nepotrebuje a restart po kazdem ulozeni je presne ta vec, kvuli ktere
  // uzivatele prestanou nastaveni pouzivat.
  if (WebConfig_TakeLocationChanged()) {
    // ADSB i CHMU si polohu ctou pri kazdem stazeni, takze staci vratit
    // obrazovku do vychoziho stavu - ta si vyzada nova data sama.
    Serial.println("Poloha zmenena, obnovuji data");
    activeEnter();
    s_forceDraw = true;
  }
  // Uzivatel mohl vypnout prave zobrazenou obrazovku. Prepnout se musi TADY,
  // ne v obsluze pozadavku - kreslit se smi jen odsud.
  if (!screenVisible(s_screen)) {
    for (int i = 0; i < SCREEN_N; i++) {
      if (!screenVisible(i)) continue;
      Serial.printf("Obrazovka %d vypnuta, prepinam na %d\n", s_screen, i);
      s_screen = i;
      Settings_SetScreen((uint8_t)s_screen);
      activeEnter();
      s_forceDraw = true;
      break;
    }
  }

  // Uzivatel si vyzadal zapomenuti site z obrazovky Nastaveni.
  if (ScreenSettings_WantsWifiReset()) {
    ScreenSettings_ClearWifiReset();
    WiFi_Reset();               // spadne na pristupovy bod a prekresli se
    return;
  }

  // Tovarni reset (z obrazovky Nastaveni nebo z webu). Konci restartem.
  if (ScreenSettings_WantsReset()) {
    ScreenSettings_ClearReset();
    doFactoryReset();
  }
  if (WebConfig_WantsFactoryReset()) doFactoryReset();

  // Nektera nastaveni se projevi az po cistem startu (ktere obrazovky
  // existuji, kde jsme). Web si o restart rekne; provest ho tady znamena, ze
  // prohlizec uz dostal svou odpoved.
  if (WebConfig_WantsRestart()) {
    Serial.println("Nastaveni zmeneno, restartuji");
    Serial.flush();
    delay(400);
    ESP.restart();
  }

  // Prekreslovani je oddelene od cteni dotyku a zastropovane na ~12 snimku/s.
  static unsigned long lastDraw = 0;
  bool wantDraw = activeTick();
  if (s_forceDraw || (wantDraw && millis() - lastDraw >= 80)) {
    drawActive();
    lastDraw = millis();
    s_forceDraw = false;
  }

  autoRotateTick();

  // Stav stridani na webovou stranku, aby slo poznat, PROC se zrovna nestrida
  // (vypnuto vs. pozastaveno po doteku).
  {
    const unsigned long now2 = millis();
    const uint32_t left = (now2 < s_touchPauseUntil)
                          ? (uint32_t)((s_touchPauseUntil - now2) / 1000UL) : 0;
    WebConfig_SetRotateInfo(Settings_AutoRotateSec(), left);
  }

  Forecast_Tick();   // predpoved, casy slunce a ovzdusi
  NightMode_Tick();  // denni / nocni jas
  Route_Tick();      // cekajici dotaz "odkud a kam leti"
  Settings_Tick();   // odlozeny zapis stavu UI do NVS
  Watchdog_Feed();
  delay(5);
}
