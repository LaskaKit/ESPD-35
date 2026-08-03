// =============================================================================
//  ESPD35_MeteoPlaneRadar - obrazovka Nastaveni.
//
//  GEOMETRIE OVLADACICH PRVKU
//  --------------------------
//  Vsechny dotykove zony jsou nize jako pojmenovane konstanty a NESMI se
//  prekryvat. Rozvrzeni je dvousloupcove:
//
//     x:   0        14                 234  250              466   480
//          |---------|------ LEVY ------|----|---- PRAVY -----|-----|
//     y=6   Nastaveni (titulek, pres celou sirku)
//     y=28  laskakit.cz | v0.3.0
//     y=48  [ posuvnik jasu            ]   [ stav site (text) ]
//     y=92                                 [ WiFi + poloha    ]
//     y=116 [ - ] [ SV ] [ + ]  + kompas
//     y=146                                 [ Firmware update  ]
//     y=176 [ Jednotky: letecke        ]
//     y=200                                 (napoveda k ovladani)
//     y=228 Poloha 50.0755, 14.4378
//     y=248                                   [ Tovarni reset ]  (cervene)
//     y=296        [ tecky prepinani obrazovek ]
//
//  Tlacitko tovarniho resetu je zamerne mensi a odsazene od ostatnich - je to
//  destruktivni akce a nema svadet k nahodnemu klepnuti. Navic vyzaduje dve
//  klepnuti (prvni natahne, druhe potvrdi).
//
//  Volny pruh nahore (y < 46) a dole (y 284..292) slouzi k prepnuti obrazovky
//  dlouhym stiskem - viz ScreenSettings_HitsControl().
// =============================================================================
#include "ScreenSettings.h"
#include "Settings.h"
#include "WiFiPortal.h"
#include "UI.h"
#include "Config.h"
#include "Version.h"

#include <WiFi.h>
#include <math.h>

// --- Posuvnik jasu (levy sloupec) ---
#define SL_X     14
#define SL_Y     58
#define SL_W    220
#define SL_H     26
// Dotykova zona je vetsi nez drazka, aby se do ni dalo trefit i nepresne.
#define SL_HIT_X0   4
#define SL_HIT_X1 244
#define SL_HIT_Y0  48
#define SL_HIT_Y1  94

// --- Orientace mapy (levy sloupec) ---
#define ROT_Y        116
#define ROT_H         44
#define COMPASS_CX    36
#define COMPASS_CY   138
#define COMPASS_R     20
#define ROT_MINUS_X   70
#define ROT_BTN_W     46
#define ROT_VAL_X    122
#define ROT_VAL_W     54
#define ROT_PLUS_X   182

// --- Tlacitko jednotek (levy sloupec) ---
#define UNITS_X   14
#define UNITS_Y  176
#define UNITS_W  220
#define UNITS_H   40

// --- Tlacitka v pravem sloupci ---
#define BTN_X    250
#define BTN_W    216
#define BTN_H     42
#define WIFI_Y    92
#define OTA_Y    146

// --- Tovarni reset: mensi, cervene, vycentrovane pod tlacitky a odsazene ---
#define RST_W    140
#define RST_H     32
#define RST_X    (BTN_X + (BTN_W - RST_W) / 2)   // stred stejny jako u tlacitek
#define RST_Y    248                             // 60 px pod tlacitkem OTA

static bool s_wantsPortal = false;
static bool s_wantsOTA    = false;
static bool s_wantsReset  = false;

// Stav potvrzeni tovarniho resetu (viz ScreenSettings.h).
static bool          s_resetArmed   = false;
static unsigned long s_resetArmedAt = 0;

// Vyprsela nabidka potvrzeni? Pouziva se pri kresleni i pri klepnuti, aby
// natazene tlacitko po chvili samo zeslo.
static bool resetArmExpired() {
  return s_resetArmed && (millis() - s_resetArmedAt > RESET_CONFIRM_MS);
}

// Kratka zkratka svetove strany. Cokoli, co neni nasobek 45 stupnu (moznost
// pri zmene MAP_ROT_STEP_DEG), spadne na vypis ve stupnich, takze radek nikdy
// neukaze nesmysl.
static const char* bearingLabel(uint16_t deg, char* buf, size_t len) {
  static const char* N8[8] = { "S", "SV", "V", "JV", "J", "JZ", "Z", "SZ" };
  if (deg % 45 == 0) return N8[(deg / 45) % 8];
  snprintf(buf, len, "%u", (unsigned)deg);
  return buf;
}

static bool inRect(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

void ScreenSettings_Enter() {
  s_resetArmed = false;   // pri kazdem prichodu zacit s "nenatazenym" resetem
}

// Vraci true = prekreslit. Jedine, co se tu meni samo, je vyprseni nabidky
// potvrzeni tovarniho resetu.
bool ScreenSettings_Tick() {
  if (resetArmExpired()) { s_resetArmed = false; return true; }
  return false;
}

bool ScreenSettings_WantsPortal() { return s_wantsPortal; }
void ScreenSettings_ClearPortal() { s_wantsPortal = false; }
bool ScreenSettings_WantsOTA()    { return s_wantsOTA; }
void ScreenSettings_ClearOTA()    { s_wantsOTA = false; }
bool ScreenSettings_WantsReset()  { return s_wantsReset; }
void ScreenSettings_ClearReset()  { s_wantsReset = false; s_resetArmed = false; }

bool ScreenSettings_HitsControl(int x, int y) {
  if (x >= SL_HIT_X0 && x <= SL_HIT_X1 && y >= SL_HIT_Y0 && y <= SL_HIT_Y1) return true;
  if (y >= ROT_Y && y < ROT_Y + ROT_H) {
    if (inRect(x, y, ROT_MINUS_X, ROT_Y, ROT_BTN_W, ROT_H)) return true;
    if (inRect(x, y, ROT_PLUS_X,  ROT_Y, ROT_BTN_W, ROT_H)) return true;
  }
  if (inRect(x, y, UNITS_X, UNITS_Y, UNITS_W, UNITS_H)) return true;
  if (inRect(x, y, BTN_X, WIFI_Y, BTN_W, BTN_H)) return true;
  if (inRect(x, y, BTN_X, OTA_Y,  BTN_W, BTN_H)) return true;
  if (inRect(x, y, RST_X, RST_Y,  RST_W, RST_H)) return true;
  return false;
}

bool ScreenSettings_HandleTap(int x, int y) {
  // --- Jas ---
  if (x >= SL_HIT_X0 && x <= SL_HIT_X1 && y >= SL_HIT_Y0 && y <= SL_HIT_Y1) {
    int pct = (x - SL_X) * 100 / SL_W;
    if (pct < 10) pct = 10;
    if (pct > 100) pct = 100;
    Settings_SetBacklight((uint8_t)pct);
    Backlight_Set((uint8_t)pct);
    return true;
  }

  // --- Orientace mapy: krok o MAP_ROT_STEP_DEG. "+" jde po smeru hodinovych
  //     rucicek (S -> SV -> V -> JV ...). ---
  if (y >= ROT_Y && y < ROT_Y + ROT_H) {
    int top = (int)Settings_TopBearing();
    if (inRect(x, y, ROT_MINUS_X, ROT_Y, ROT_BTN_W, ROT_H)) {
      top = (top - MAP_ROT_STEP_DEG + 360) % 360;
      Settings_SetTopBearing((uint16_t)top);
      return true;
    }
    if (inRect(x, y, ROT_PLUS_X, ROT_Y, ROT_BTN_W, ROT_H)) {
      top = (top + MAP_ROT_STEP_DEG) % 360;
      Settings_SetTopBearing((uint16_t)top);
      return true;
    }
  }

  // --- Jednotky ---
  if (inRect(x, y, UNITS_X, UNITS_Y, UNITS_W, UNITS_H)) {
    Settings_SetMetricUnits(!Settings_MetricUnits());
    return true;
  }

  // --- WiFi + poloha (AP portal) ---
  if (inRect(x, y, BTN_X, WIFI_Y, BTN_W, BTN_H)) {
    s_wantsPortal = true;
    return true;
  }

  // --- Aktualizace firmwaru pres WiFi ---
  if (inRect(x, y, BTN_X, OTA_Y, BTN_W, BTN_H)) {
    s_wantsOTA = true;
    return true;
  }

  // --- Tovarni reset: dve klepnuti ---
  if (inRect(x, y, RST_X, RST_Y, RST_W, RST_H)) {
    if (s_resetArmed && !resetArmExpired()) {
      s_wantsReset = true;             // potvrzeno -> provede .ino
    } else {
      s_resetArmed = true;             // natahnout a cekat na potvrzeni
      s_resetArmedAt = millis();
    }
    return true;
  }

  // Klepnuti kamkoli jinam natazeny reset zrusi - je to nejprirozenejsi
  // zpusob, jak z toho couvnout.
  if (s_resetArmed) { s_resetArmed = false; return true; }

  return false;
}

// Tlacitko: vyplneny zaobleny obdelnik s vycentrovanym popiskem.
// fg = barva popisku; na tmavem pozadi (cervena) je potreba bila, na svetlem
// (azurova, zluta, zelena) cerna.
static void drawButton(int bx, int by, int bw, int bh, uint16_t bg,
                       const char* label, uint8_t size, uint16_t fg = C_BLACK) {
  gfx->fillRoundRect(bx, by, bw, bh, 6, bg);
  gfx->drawRoundRect(bx, by, bw, bh, 6, C_WHITE);
  gfx->setTextSize(size); gfx->setTextColor(fg);
  int tw = strlen(label) * 6 * size;
  int lx = bx + (bw - tw) / 2; if (lx < bx + 2) lx = bx + 2;
  gfx->setCursor(lx, by + (bh - 8 * size) / 2);
  gfx->print(label);
}

// Kompasovy nahled: kruh s ryskou ukazujici, kde je po otoceni SEVER.
// Diky nemu je nastaveni videt hned, bez prepinani na radar.
static void drawCompass() {
  gfx->drawCircle(COMPASS_CX, COMPASS_CY, COMPASS_R, C_GRAY);
  float th = -(float)Settings_TopBearing() * 0.0174532925f;   // smer severu
  int nx = COMPASS_CX + (int)lroundf((COMPASS_R - 4) * sinf(th));
  int ny = COMPASS_CY - (int)lroundf((COMPASS_R - 4) * cosf(th));
  gfx->drawLine(COMPASS_CX, COMPASS_CY, nx, ny, C_RED);
  gfx->fillCircle(COMPASS_CX, COMPASS_CY, 2, C_WHITE);
  gfx->setTextSize(1); gfx->setTextColor(C_RED);
  gfx->setCursor(nx - 2, ny - 3); gfx->print("S");
}

void ScreenSettings_Draw() {
  gfx->fillScreen(C_BLACK);

  // --- Titulek ---
  UI_TextCentered("Nastaveni", 6, C_CYAN, 2);
  {
    char v[40];
    snprintf(v, sizeof(v), "laskakit.cz  |  verze %s", FW_VERSION);
    UI_TextCentered(v, 28, C_GRAY, 1);
  }

  // ------------------------------- LEVY SLOUPEC -----------------------------
  // Jas
  gfx->setTextSize(1); gfx->setTextColor(C_WHITE);
  gfx->setCursor(SL_X, 46); gfx->print("Jas");
  {
    int pct = Settings_Backlight();
    gfx->drawRoundRect(SL_X, SL_Y, SL_W, SL_H, 4, C_GRAY);
    int fill = (SL_W - 4) * pct / 100;
    gfx->fillRoundRect(SL_X + 2, SL_Y + 2, fill, SL_H - 4, 3, C_CYAN);
    char b[8]; snprintf(b, sizeof(b), "%d%%", pct);
    gfx->setTextSize(1); gfx->setTextColor(C_WHITE);
    gfx->setCursor(SL_X + SL_W - 34, SL_Y + 9); gfx->print(b);
  }

  // Orientace mapy
  gfx->setTextSize(1); gfx->setTextColor(C_WHITE);
  gfx->setCursor(SL_X, 100); gfx->print("Nahore (smer pohledu z okna)");
  drawCompass();
  drawButton(ROT_MINUS_X, ROT_Y, ROT_BTN_W, ROT_H, C_GRAY, "-", 2);
  drawButton(ROT_PLUS_X,  ROT_Y, ROT_BTN_W, ROT_H, C_GRAY, "+", 2);
  {
    char buf[8];
    const char* lbl = bearingLabel(Settings_TopBearing(), buf, sizeof(buf));
    gfx->drawRoundRect(ROT_VAL_X, ROT_Y, ROT_VAL_W, ROT_H, 6, C_DKGRAY);
    UI_TextCenteredIn(lbl, ROT_VAL_X, ROT_VAL_W, ROT_Y + ROT_H / 2 - 8, C_YELLOW, 2);
  }

  // Jednotky
  drawButton(UNITS_X, UNITS_Y, UNITS_W, UNITS_H, C_CYAN,
             Settings_MetricUnits() ? "Jednotky: metricke" : "Jednotky: letecke", 1);

  // Poloha
  {
    char line[40];
    gfx->setTextSize(1); gfx->setTextColor(C_GRAY);
    gfx->setCursor(SL_X, 228); gfx->print("Poloha");
    snprintf(line, sizeof(line), "%.4f, %.4f", Settings_Lat(), Settings_Lon());
    gfx->setTextColor(C_WHITE);
    gfx->setCursor(SL_X, 242); gfx->print(line);
    gfx->setTextColor(C_GRAY);
    gfx->setCursor(SL_X, 256);
    gfx->print(Settings_HasLocation() ? "(ulozena)" : "(vychozi)");
  }

  // ------------------------------- PRAVY SLOUPEC ----------------------------
  {
    gfx->setTextSize(1); gfx->setTextColor(C_GRAY);
    gfx->setCursor(BTN_X, 46); gfx->print("Sit");
    gfx->setTextColor(WiFi_IsConnected() ? C_GREEN : C_YELLOW);
    gfx->setCursor(BTN_X, 60); gfx->print(WiFi_SSID());
    gfx->setTextColor(C_WHITE);
    gfx->setCursor(BTN_X, 74); gfx->print(WiFi_IP());
  }

  drawButton(BTN_X, WIFI_Y, BTN_W, BTN_H, C_YELLOW, "WiFi + poloha (AP)", 1);
  drawButton(BTN_X, OTA_Y,  BTN_W, BTN_H, C_GREEN,  "Firmware update (OTA)", 1);

  gfx->setTextSize(1); gfx->setTextColor(C_GRAY);
  gfx->setCursor(BTN_X, 200); gfx->print("Prepnuti obrazovky: klepnuti na");
  gfx->setCursor(BTN_X, 214); gfx->print("tecky dole, nebo dlouhy stisk");
  gfx->setCursor(BTN_X, 228); gfx->print("v leve / prave polovine.");

  // --- Tovarni reset ---
  // Mensi nez ostatni tlacitka a odsazeny, aby na nej nesla trefit ruka
  // mirici na "Firmware update". Druhe klepnuti (do RESET_CONFIRM_MS) potvrdi.
  if (s_resetArmed && !resetArmExpired()) {
    drawButton(RST_X, RST_Y, RST_W, RST_H, C_YELLOW, "Opravdu? Klepni", 1);
    UI_TextCenteredIn("Smaze WiFi i nastaveni", BTN_X, BTN_W,
                      RST_Y + RST_H + 4, C_YELLOW, 1);
  } else {
    drawButton(RST_X, RST_Y, RST_W, RST_H, C_RED, "Tovarni reset", 1, C_WHITE);
  }
}
