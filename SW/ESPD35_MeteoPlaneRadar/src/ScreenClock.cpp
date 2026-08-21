// =============================================================================
//  ESPD35_MeteoPlaneRadar - obrazovka hodin. Viz ScreenClock.h.
// =============================================================================
#include "ScreenClock.h"
#include "Settings.h"
#include "Outside.h"
#include "Forecast.h"
#include "WxIcon.h"
#include "NightMode.h"
#include "Layout.h"
#include "Lang.h"
#include "UI.h"
#include "Config.h"

#include <time.h>
#include <math.h>
#include <stdio.h>

// --- Rozvrzeni 480x320 -------------------------------------------------------
// Cas ve velikosti 8 je 48x64 px na znak, takze "12:34" = 240 px - presne
// polovina sirky, hezky vycentrovane a zbyde misto na pocasi pod tim.
//
// Vsechny svisle pozice jsou voleny tak, aby text nekolidoval s elipsou behu
// sekund. Kde ke kolizi presto dojit muze (datum - elipsa je u horniho okraje
// nejsirsi), kresli se text s NEPRUHLEDNYM pozadim, takze si kolem sebe udela
// misto. Viz UI_TextCenteredBg.
#define CLK_DATE_Y    30
#define CLK_TIME_Y    58
#define CLK_TIME_SZ    8
#define CLK_WIND_Y   132      // vitr hned pod hodinami, velikost 2
#define CLK_WX_CX    150
#define CLK_WX_CY    206
#define CLK_WX_R      32
#define CLK_TEMP_X   198
#define CLK_TEMP_Y   190
#define CLK_PRECIP_Y 222
#define CLK_SUN_Y    258      // velikost 2, svetle seda - musi byt CITELNE

static int s_lastSec = -1;

void ScreenClock_Enter() { s_lastSec = -1; }

// Prekresluje se jednou za sekundu - to je jediné, co se na teto obrazovce
// bezne meni. Bez teto podminky by se cely snimek (300 kB po SPI) posilal
// dvanactkrat za sekundu uplne zbytecne.
bool ScreenClock_Tick() {
  if (!Outside_TimeValid()) return false;
  time_t now = time(nullptr);
  struct tm lt;
  localtime_r(&now, &lt);
  if (lt.tm_sec == s_lastSec) return false;
  s_lastSec = lt.tm_sec;
  return true;
}

bool ScreenClock_HandleTap(int x, int y) {
  (void)x; (void)y;
  if (Settings_NightAuto()) return false;   // automatika by to hned vratila
  NightMode_Toggle();
  return true;
}

// -----------------------------------------------------------------------------
//  Beh sekund po ELIPSE
//
//  Do 0.3.0 to byl beh po samem obvodu ramu. V krabicce to nefungovalo -
//  ramecek prekryva par pixelu po okrajich, takze z behu byla videt sotva
//  polovina. Elipsa vepsana do displeje s odsazenim (CLK_SEC_INSET_*) lezi
//  cela ve viditelne plose a navic se blizi puvodnimu prstenci z kulateho
//  displeje, ze ktereho tenhle projekt vysel.
//
//  Pozice se pocita parametrickym uhlem od DVANACTKY (horni stred) po smeru
//  hodinovych rucicek. Rozestupy podel elipsy tim nejsou uplne stejne dlouhe,
//  ale presne takhle se chova i rucicka na oválnych hodinach - oko to necte
//  jako chybu, kdezto skokova zmena rychlosti v rozich obdelniku ano.
// -----------------------------------------------------------------------------
#define CLK_SEC_CX (LCD_WIDTH / 2)
#define CLK_SEC_CY (LCD_HEIGHT / 2)
#define CLK_SEC_RX (LCD_WIDTH / 2 - CLK_SEC_INSET_X)
#define CLK_SEC_RY (LCD_HEIGHT / 2 - CLK_SEC_INSET_Y)

// Bod na elipse pro zlomek otacky (0 = dvanactka, 1 = zpet na dvanactce).
static void ellipsePoint(float frac, int* px, int* py) {
  const float a = frac * 6.2831853f - 1.5707963f;    // -90 deg = dvanactka
  *px = CLK_SEC_CX + (int)lroundf(CLK_SEC_RX * cosf(a));
  *py = CLK_SEC_CY + (int)lroundf(CLK_SEC_RY * sinf(a));
}

// Ztlumeni barvy RGB565 na num/den puvodniho jasu. Kazda slozka se skaluje
// zvlast ve svem rozsahu, jinak by ohon pri hasnuti menil odstin.
static uint16_t fade565(uint16_t c, int num, int den) {
  if (num >= den) return c;
  if (num <= 0)   return 0;
  const uint16_t r = (uint16_t)((((c >> 11) & 0x1F) * num) / den);
  const uint16_t g = (uint16_t)((((c >>  5) & 0x3F) * num) / den);
  const uint16_t b = (uint16_t)((( c        & 0x1F) * num) / den);
  return (uint16_t)((r << 11) | (g << 5) | b);
}

static void ellipseMark(float frac, int r, uint16_t color) {
  int x, y;
  ellipsePoint(frac, &x, &y);
  gfx->fillCircle(x, y, r, color);
}

static void drawSeconds(int sec) {
  const uint8_t style = Settings_SecondsStyle();
  if (style == SEC_STYLE_OFF) return;
  const uint16_t col = Settings_SecondsColor();
  const float now = (float)sec / 60.0f;

  if (style == SEC_STYLE_DOTS) {
    // Vsech 60 poloh slabe, ubehnute vyrazne. Prazdna elipsa s jednou teckou
    // nerika, kolik uz ubehlo; tohle ano.
    for (int i = 0; i < 60; i++)
      ellipseMark((float)i / 60.0f, (i % 5 == 0) ? 2 : 1, C_DKGRAY);
    for (int i = 0; i <= sec; i++)
      ellipseMark((float)i / 60.0f, CLK_SEC_TH / 2, col);
    return;
  }

  if (style == SEC_STYLE_LINE) {
    // "Cara": plny oblouk od dvanactky k aktualni sekunde. Krok 1/720 otacky
    // je dost jemny, aby na nejsirsim miste elipsy nevznikaly dirky.
    for (int i = 0; i <= sec * 12; i++)
      ellipseMark((float)i / 720.0f, CLK_SEC_TH / 2, col);
    return;
  }

  // "Kometa": hlava s dohasinajicim ohonem.
  //
  // Ohon je dlouhy CLK_COMET_TAIL_SEC sekund, kresleny po ctvrtsekundovych
  // krocich, aby byl souvisly. Prvni verze mela deset kroku po pul sekunde,
  // tedy pet sekund - z devadesati procent elipsy nebylo videt nic a efekt
  // se ztracel. Jas klesa LINEARNE po celé delce; pulení jasu po clancich
  // zhasinalo prilis rychle na to, aby delsi ohon vubec mel smysl.
  const int steps = CLK_COMET_TAIL_SEC * 4;
  for (int i = steps; i >= 0; i--) {          // od konce ohonu k hlave
    float frac = now - (float)i / 240.0f;     // 240 = 60 s x 4 kroky
    if (frac < 0) frac += 1.0f;
    const uint16_t c = fade565(col, steps - i + 1, steps + 1);
    ellipseMark(frac, (i == 0) ? CLK_SEC_TH / 2 + 1 : CLK_SEC_TH / 2, c);
  }
}

// -----------------------------------------------------------------------------
void ScreenClock_Draw() {
  gfx->fillScreen(C_BLACK);
  Layout_Begin();
  Layout_Reserve(LY_DOTS_X, LY_DOTS_Y0, LY_DOTS_W, LY_DOTS_H);

  if (!Outside_TimeValid()) {
    UI_TextCentered(T(S_WIFI_WAIT), LCD_HEIGHT / 2 - 20, C_GRAY, 2);
    UI_TextCentered("cekam na cas", LCD_HEIGHT / 2 + 10, C_GRAY, 1);
    return;
  }

  time_t now = time(nullptr);
  struct tm lt;
  localtime_r(&now, &lt);

  // Beh sekund se kresli JAKO PRVNI a texty pres nej, aby elipsa vedla za
  // nimi a ne skrz. Opacne poradi by znamenalo carou preskrtnute datum.
  drawSeconds(lt.tm_sec);

  // --- Datum ---------------------------------------------------------------
  {
    char d[40];
    snprintf(d, sizeof(d), "%s %d. %s %d",
             Lang_WeekdayShort(lt.tm_wday), lt.tm_mday,
             Lang_MonthName(lt.tm_mon), lt.tm_year + 1900);
    // S podkladem: u horniho okraje je elipsa nejsirsi a prochazela by presne
    // tudy, kde je datum.
    UI_TextCenteredBg(d, CLK_DATE_Y, C_LTGRAY, C_BLACK, 2);
  }

  // --- Cas -----------------------------------------------------------------
  {
    char t[8];
    snprintf(t, sizeof(t), "%02d:%02d", lt.tm_hour, lt.tm_min);
    UI_TextCentered(t, CLK_TIME_Y, Settings_ClockColor(), CLK_TIME_SZ);
  }

  // --- Vitr ----------------------------------------------------------------
  // Hned pod hodinami a velikosti 2: driv byl schovany az dole pod teplotou
  // velikosti 1, kde ho nebylo poznat.
  if (Forecast_CurrentValid()) {
    char w[24];
    snprintf(w, sizeof(w), "vitr %.0f km/h", Forecast_CurrentWind());
    UI_TextCenteredBg(w, CLK_WIND_Y, C_LTGRAY, C_BLACK, 2);
  }

  // --- Pocasi --------------------------------------------------------------
  // Vsechno z bloku "current" teze odpovedi, kterou stahuje predpoved.
  if (Forecast_CurrentValid()) {
    const int code = Forecast_CurrentCode();
    // Ikona ma v noci mesic misto slunce - a "noc" se pozna podle skutecnych
    // casu slunce, ne podle hodiny.
    bool night = Settings_IsNight();
    time_t rise, set;
    if (Forecast_SunTimes(&rise, &set)) night = (now < rise || now >= set);

    WxIcon_Draw(CLK_WX_CX, CLK_WX_CY, CLK_WX_R, code, night);

    char temp[16];
    snprintf(temp, sizeof(temp), "%d %s",
             (int)lroundf(Forecast_CurrentTemp()), OUTSIDE_DEG_TEXT);
    gfx->setTextSize(3);
    gfx->setTextColor(WxIcon_Color(code), C_BLACK);
    gfx->setCursor(CLK_TEMP_X, CLK_TEMP_Y);
    gfx->print(temp);

    // Srazky jen kdyz nejake jsou - "0.0 mm" je zbytecny radek. Vitr uz je
    // nahore pod hodinami.
    const float mm = Forecast_CurrentPrecip();
    if (mm > 0.05f) {
      char p[16];
      snprintf(p, sizeof(p), "%.1f mm", mm);
      gfx->setTextSize(2);
      gfx->setTextColor(C_CYAN, C_BLACK);
      gfx->setCursor(CLK_TEMP_X, CLK_PRECIP_Y);
      gfx->print(p);
    }
  } else if (Outside_TempValid()) {
    // Predpoved jeste nedosla, ale teplotu uz mame odjinud.
    char temp[16];
    snprintf(temp, sizeof(temp), "%d %s",
             (int)lroundf(Outside_Temp()), OUTSIDE_DEG_TEXT);
    UI_TextCentered(temp, CLK_TEMP_Y, C_WHITE, 3);
  }

  // --- Vychod a zapad slunce ------------------------------------------------
  {
    time_t rise, set;
    if (Forecast_SunTimes(&rise, &set)) {
      struct tm r, s;
      localtime_r(&rise, &r);
      localtime_r(&set, &s);
      char line[48];
      snprintf(line, sizeof(line), "vychod %02d:%02d   zapad %02d:%02d",
               r.tm_hour, r.tm_min, s.tm_hour, s.tm_min);
      // Bylo C_DKGRAY velikosti 1 - na cernem pozadi to proste neslo precist.
      UI_TextCenteredBg(line, CLK_SUN_Y, C_LTGRAY, C_BLACK, 2);
    }
  }
}
