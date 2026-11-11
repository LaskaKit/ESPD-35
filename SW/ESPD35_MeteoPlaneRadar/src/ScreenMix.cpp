// =============================================================================
//  ESPD35_MeteoPlaneRadar - obrazovka vyroby elektriny. Viz ScreenMix.h.
// =============================================================================
#include "ScreenMix.h"
#include "Energy.h"
#include "Settings.h"
#include "Outside.h"
#include "Layout.h"
#include "Lang.h"
#include "UI.h"
#include "Config.h"

#include <WiFi.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

// --- Rozvrzeni 480x320 -------------------------------------------------------
// Na kulatem displeji visi popisky kolem prstence a hledaji si misto po
// obvodu. Na obdelniku je vedle prstence volny pruh, takze popisky jsou
// legenda: barevny ctverecek, nazev a procenta pod sebou, serazene podle
// podilu. Cte se to jako tabulka a nic se neprekryva.
#define MCX      130
#define MCY      170
#define R_OUT    112      // horni okraj 58, dolni 282
#define R_IN      78

#define LX0      262      // legenda
#define LX1      (LCD_WIDTH - 8)
#define L_Y0      38
#define L_ROW     22
#define L_SEP_Y  (L_Y0 + MIX_GROUPS * L_ROW + 2)      // 216
#define LOAD_Y   (L_SEP_Y + 8)                        // 224
#define TRADE_Y  (LOAD_Y + 22)                        // 246
#define TIME_Y   (TRADE_Y + 24)                       // 270

// Podily pod touto hranici se v legende nevypisuji - vysec je stejne jen
// tenka linka a radek by jen odsunul ostatni.
#define LABEL_MIN_PCT 0.5f

// Barvy skupin. Stejne jako v petus/MeteoPlaneRadar: zdroj ma vsude, kde se
// o elektrine mluvi, svou "obvyklou" barvu - uhli hnede, slunce zlute.
static const uint16_t GROUP_COLOR[MIX_GROUPS] = {
  0x92DB,   // jadro    - fialova
  0x836B,   // uhli     - hneda
  0xF465,   // plyn     - oranzova
  0xFE85,   // slunce   - zluta
  0x5DFE,   // vitr     - svetle modra
  0x2B7B,   // voda     - modra
  0x5DAA,   // biomasa  - zelena
  C_GRAY    // ostatni
};

// "Vyroba CR" pro cesky vychozi stav, jinak "Vyroba SK", "Vyroba EU"... -
// obrazovka, ktera ukazuje cizi zemi a nerekne to, je horsi nez zadna.
static const char* mixTitle(char* buf, size_t cap) {
  const char* cc = Settings_MixCountry();
  if (!strcmp(cc, "cz")) return T(S_MIX);
  snprintf(buf, cap, "%s %c%c", T(S_MIX_GEN),
           (char)toupper((unsigned char)cc[0]), (char)toupper((unsigned char)cc[1]));
  return buf;
}

static bool   s_lastValid = false;
static time_t s_lastAt = 0;

void ScreenMix_Enter() { s_lastAt = 0; }

bool ScreenMix_Tick() {
  bool want = false;
  const bool valid = Energy_MixValid();
  const time_t at = Energy_MixSampleTime();
  if (valid != s_lastValid || at != s_lastAt) {
    s_lastValid = valid;
    s_lastAt = at;
    want = true;
  }
  if (UI_StatusLineChanged()) want = true;
  return want;
}

// 1234 MW -> "1,2 GW", 850 -> "850 MW". Desetinna carka, jako vsude v cestine.
static void fmtPower(float mw, char* out, size_t cap) {
  if (isnan(mw)) { snprintf(out, cap, "--"); return; }
  if (fabsf(mw) >= 1000.0f) snprintf(out, cap, "%.1f GW", mw / 1000.0f);
  else                      snprintf(out, cap, "%.0f MW", mw);
  for (char* p = out; *p; p++) if (*p == '.') *p = ',';
}

// --- Prstenec ---------------------------------------------------------------
// Poradi skupin je pevne (MixGroup), ne podle velikosti: prstenec, jehoz vysece
// mezi obnovenimi preskakuji, je necitelny, i kdyz je kazde cislo spravne.
static void drawRing() {
  const float total = Energy_MixTotalMw();
  if (total <= 0) return;

  float a = -90.0f * 0.0174532925f;           // zacina se nahore
  for (int g = 0; g < MIX_GROUPS; g++) {
    const float mw = Energy_MixMw(g);
    if (mw <= 0) continue;
    const float sweep = (mw / total) * 2.0f * 3.14159265f;

    // Tenka cerna mezera mezi vysecemi, u drobnych vysecu uzsi, aby je
    // nespolkla.
    float gap = 0.6f * 0.0174532925f;
    if (gap > sweep / 3.0f) gap = sweep / 3.0f;

    const float a0 = a + gap * 0.5f;
    const float a1 = a + sweep - gap * 0.5f;
    if (a1 > a0) UI_FillRing(MCX, MCY, R_IN, R_OUT, a0, a1, GROUP_COLOR[g]);
    a += sweep;
  }
}

// --- Stred prstence ---------------------------------------------------------
static void drawCentre() {
  char buf[24];
  const float renew = Energy_RenewableShare();
  if (renew >= 0) {
    UI_TextCenteredIn(T(S_RENEWABLE), MCX - R_IN, 2 * R_IN, MCY - 40, C_GRAY, 2);
    snprintf(buf, sizeof(buf), "%.0f%%", renew);
    UI_TextCenteredIn(buf, MCX - R_IN, 2 * R_IN, MCY - 18, C_GREEN, 4);
  }
  // Celkovy vykon pod procenty - kdyz zdroj podil OZE neposlal, je to jedine
  // cislo uprostred.
  fmtPower(Energy_MixTotalMw(), buf, sizeof(buf));
  UI_TextCenteredIn(buf, MCX - R_IN, 2 * R_IN, MCY + (renew >= 0 ? 22 : -8),
                    C_WHITE, renew >= 0 ? 2 : 3);
}

// --- Legenda ----------------------------------------------------------------
static void drawLegend() {
  struct Row { uint8_t g; float pct; };
  Row rows[MIX_GROUPS];
  int n = 0;
  for (int g = 0; g < MIX_GROUPS; g++) {
    if (Energy_MixMw(g) <= 0) continue;
    const float pct = Energy_MixShare(g);
    if (pct < LABEL_MIN_PCT) continue;
    rows[n].g = (uint8_t)g;
    rows[n].pct = pct;
    n++;
  }
  // Podle podilu sestupne - v legende se hleda "co jede nejvic".
  for (int i = 1; i < n; i++) {
    Row k = rows[i];
    int j = i - 1;
    while (j >= 0 && rows[j].pct < k.pct) { rows[j + 1] = rows[j]; j--; }
    rows[j + 1] = k;
  }

  for (int i = 0; i < n; i++) {
    const int y = L_Y0 + i * L_ROW;
    const uint8_t g = rows[i].g;
    gfx->fillRect(LX0, y + 1, 14, 14, GROUP_COLOR[g]);

    gfx->setTextSize(2);
    gfx->setTextColor(C_WHITE);
    gfx->setCursor(LX0 + 22, y);
    gfx->print(Energy_GroupName(g));

    char val[8];
    if (rows[i].pct < 0.95f) snprintf(val, sizeof(val), "<1%%");
    else                     snprintf(val, sizeof(val), "%.0f%%", rows[i].pct);
    gfx->setTextColor(GROUP_COLOR[g] == C_GRAY ? C_LTGRAY : GROUP_COLOR[g]);
    gfx->setCursor(LX1 - Layout_TextW(val, 2), y);
    gfx->print(val);
  }
}

// Spotreba, saldo a cas vzorku pod legendou.
static void drawFooter() {
  char val[24], line[40];
  gfx->drawFastHLine(LX0, L_SEP_Y, LX1 - LX0, C_DKGRAY);

  const float load = Energy_LoadMw();
  if (!isnan(load)) {
    fmtPower(load, val, sizeof(val));
    snprintf(line, sizeof(line), "%s %s", T(S_CONSUMPTION), val);
    gfx->setTextSize(2); gfx->setTextColor(C_WHITE);
    gfx->setCursor(LX0, LOAD_Y); gfx->print(line);
  }

  // Kladne = zeme vyvazi. Barva rika smer bez cteni slova.
  const float ex = Energy_ExportMw();
  if (!isnan(ex)) {
    fmtPower(fabsf(ex), val, sizeof(val));
    snprintf(line, sizeof(line), "%s %s", ex >= 0 ? T(S_EXPORT) : T(S_IMPORT), val);
    gfx->setTextSize(2); gfx->setTextColor(ex >= 0 ? C_GREEN : C_ORANGE);
    gfx->setCursor(LX0, TRADE_Y); gfx->print(line);
  }

  // Data se zverejnuji se zpozdenim jedne az dvou hodin. Cas vzorku proto
  // stoji primo pod cisly - s hodinami v hlavicce se neshoduje a nema.
  const time_t at = Energy_MixSampleTime();
  if (at && Outside_TimeValid()) {
    struct tm lt; localtime_r(&at, &lt);
    snprintf(line, sizeof(line), "%s %02d:%02d", T(S_DATA_FROM), lt.tm_hour, lt.tm_min);
    gfx->setTextSize(1); gfx->setTextColor(C_GRAY);
    gfx->setCursor(LX0, TIME_Y); gfx->print(line);
  }
}

void ScreenMix_Draw() {
  gfx->fillScreen(C_BLACK);
  Layout_Begin();
  Layout_Reserve(LY_DOTS_X, LY_DOTS_Y0, LY_DOTS_W, LY_DOTS_H);

  char title[24];
  UI_DrawHeader(mixTitle(title, sizeof(title)), C_CYAN);
  Layout_ReserveBand(UI_HDR_Y - 2, UI_HDR_H);

  if (!Energy_MixInArea()) {
    UI_TextCentered(T(S_MIX_EU_ONLY), LCD_HEIGHT / 2 - 10, C_YELLOW, 2);
    UI_TextCentered(T(S_OUT_OF_AREA), LCD_HEIGHT / 2 + 18, C_GRAY, 1);
    return;
  }
  if (!Energy_MixValid()) {
    UI_TextCentered(WiFi.status() == WL_CONNECTED ? T(S_LOADING) : T(S_WIFI_WAIT),
                    LCD_HEIGHT / 2 - 10, C_YELLOW, 2);
    UI_TextCentered("energy-charts.info (ENTSO-E)", LCD_HEIGHT / 2 + 18, C_GRAY, 1);
    return;
  }

  drawRing();
  drawCentre();
  drawLegend();
  drawFooter();
}
