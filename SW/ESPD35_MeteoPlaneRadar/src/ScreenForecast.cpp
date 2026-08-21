// =============================================================================
//  ESPD35_MeteoPlaneRadar - obrazovka predpovedi. Viz ScreenForecast.h.
// =============================================================================
#include "ScreenForecast.h"
#include "Forecast.h"
#include "Outside.h"
#include "Settings.h"
#include "WxIcon.h"
#include "Layout.h"
#include "Lang.h"
#include "UI.h"
#include "Config.h"

#include <time.h>
#include <math.h>
#include <stdio.h>

// --- Rozvrzeni 480x320 -------------------------------------------------------
// SVISLE ROZESTUPY MUSI POCITAT S REZERVACI MISTA, ne jen s vyskou pisma.
// Skupina "hodnota + jednotka" narokuje LY_CHAR_H(2) + 4 = 20 px a zacina
// dva pixely nad textem, takze dva radky pod sebou potrebuji rozestup
// nejmene 20 px a nasledujici cara nejmene 20 px pod poslednim radkem.
//
// Prvni verze mela srazky na 102 (narok 100 az 122) a oddelovac na 118 -
// cara tedy narok prekryla a SRAZKY SE VUBEC NEKRESLILY. Layout to udelal
// spravne, chyba byla v rozvrzeni. Rozestupy nize kontroluje test.
#define FC_HDR_Y       6
#define FC_SEP1_Y     30
#define FC_HRS_LABEL  36      // "14h"        velikost 1
#define FC_HRS_ICON   64      // stred ikony
#define FC_HRS_TEMP   84      // teplota      velikost 2  (narok  82..102)
#define FC_HRS_RAIN  104      // srazky       velikost 2  (narok 102..122)
#define FC_HRS_W     (LCD_WIDTH / FORECAST_HOURS)   // 480 / 6 = 80
#define FC_SEP2_Y    126

#define FC_DAY_Y0    132
#define FC_DAY_H      42      // 3 x 42 = 126, konci na 258
#define FC_SEP3_Y    262

// Radek ovzdusi: maly popisek nahore, VELKA hodnota pod nim. Konci na 292,
// tecky prepinani obrazovek zacinaji na 298.
#define FC_AQ_LABEL  266
#define FC_AQ_VALUE  276

// Sloupce denniho radku. Sloupecek srazek (modry obdelnik vpravo) ve verzi
// 0.4.0 ZMIZEL - nemel popisek ani meritko, takze z nej neslo poznat nic, co
// by uz nerikalo cislo vedle. Uvolnene misto dostala cisla.
#define FC_D_NAME_X   10
#define FC_D_ICON_X  105
#define FC_D_TEMP_X  140
#define FC_D_RAIN_X  265
#define FC_D_WIND_X  375

static unsigned long s_lastDraw = 0;

void ScreenForecast_Enter() { s_lastDraw = 0; }

// Predpoved se meni po pulhodinach, takze prekreslovat casteji nez jednou za
// deset sekund nema co ukazat. Perioda navic pokryva prechod hodiny, po kterem
// se posune prvni sloupec.
bool ScreenForecast_Tick() {
  unsigned long now = millis();
  if (s_lastDraw && now - s_lastDraw < 10000UL) return false;
  s_lastDraw = now;
  return true;
}

// Text, ktery se nakresli jen kdyz je pro nej misto. Vraci, jestli se kreslil.
static bool textIfFree(const char* s, int x, int y, uint16_t color, uint8_t size) {
  const int w = Layout_TextW(s, size);
  if (!Layout_Claim(x - 2, y - 2, w + 4, LY_CHAR_H(size) + 4)) return false;
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  gfx->setCursor(x, y);
  gfx->print(s);
  return true;
}

static void textCenteredIfFree(const char* s, int cx, int y, uint16_t color, uint8_t size) {
  textIfFree(s, cx - Layout_TextW(s, size) / 2, y, color, size);
}

// Text BEZ narokovani mista. Pouziva se uvnitr skupin, ktere si uz misto
// zabraly jako celek - viz valueWithUnit().
static void textRaw(const char* s, int x, int y, uint16_t color, uint8_t size) {
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  gfx->setCursor(x, y);
  gfx->print(s);
}

// VELKA hodnota (velikost 2) a za ni MALA jednotka (velikost 1), usazena na
// spodni hranu cisla. Cislo je to, co clovek hleda; "mm" nebo "km/h" jen
// upresnuje. Vraci x hned za jednotkou.
//
// POZOR NA REZERVACI MISTA. Prvni verze narokovala hodnotu a jednotku zvlast
// pres textIfFree() - jenze obdelnik jednotky (y+8) lezi UVNITR obdelniku
// hodnoty (y-2 az y+22), takze Layout_Claim() druhy narok vzdy odmitl
// a jednotka se nikdy nenakreslila. Stejna chyba brala hodnoty v radku
// ovzdusi, kde se obdelnik popisku prekryval s obdelnikem hodnoty pod nim.
//
// Skupina proto narokuje JEDEN obdelnik pres vsechno a uvnitr uz se kresli
// primo. To je i spravne vecne: hodnota bez jednotky nebo popisek bez hodnoty
// nemaji smysl - bud se vejde cela skupina, nebo se nekresli nic.
static int valueWithUnit(int x, int y, const char* value, const char* unit,
                         uint16_t vColor, uint16_t uColor) {
  const int vw = Layout_TextW(value, 2);
  const int uw = (unit && unit[0]) ? Layout_TextW(unit, 1) : 0;
  const int total = vw + (uw ? 3 + uw : 0);

  if (!Layout_Claim(x - 2, y - 2, total + 4, LY_CHAR_H(2) + 4)) return x;

  textRaw(value, x, y, vColor, 2);
  // +8 = rozdil vysky pisma velikosti 2 a 1, aby jednotka sedela dole.
  if (uw) textRaw(unit, x + vw + 3, y + 8, uColor, 1);
  return x + total;
}

// Vycentrovana dvojice hodnota + jednotka (hodinove sloupce).
static void valueWithUnitCentered(int cx, int y, const char* value, const char* unit,
                                  uint16_t vColor, uint16_t uColor) {
  const int vw = Layout_TextW(value, 2);
  const int uw = (unit && unit[0]) ? Layout_TextW(unit, 1) : 0;
  const int total = vw + (uw ? 2 + uw : 0);
  valueWithUnit(cx - total / 2, y, value, unit, vColor, uColor);
}

// Skupina "maly popisek nahore, velka hodnota pod nim" pro radek ovzdusi.
// Naroky obou radku by se pretly, takze se zabira jeden obdelnik pres oba.
static void labelledValue(int x, int yLabel, int yValue, const char* label,
                          const char* value, const char* unit,
                          uint16_t vColor, uint16_t uColor) {
  const int vw = Layout_TextW(value, 2);
  const int uw = (unit && unit[0]) ? Layout_TextW(unit, 1) : 0;
  int w = vw + (uw ? 3 + uw : 0);
  const int lw = Layout_TextW(label, 1);
  if (lw > w) w = lw;

  if (!Layout_Claim(x - 2, yLabel - 2, w + 4, (yValue - yLabel) + LY_CHAR_H(2) + 4)) return;

  textRaw(label, x, yLabel, C_GRAY, 1);
  textRaw(value, x, yValue, vColor, 2);
  if (uw) textRaw(unit, x + vw + 3, yValue + 8, uColor, 1);
}

// Barva podle TEPLOTY, ne podle stavu pocasi.
//
// Driv se teplota barvila pres WxIcon_Color(), tedy podle toho, jestli prsi
// nebo sviti slunce - jenze barvu u cisla ve stupnich kazdy cte jako teplotu.
// Vysledek vypadal nahodne: 18 stupnu cervene, 22 modre. Stav pocasi uz nese
// ikona nad cislem, takze barva muze delat to, co se od ni ceka.
static uint16_t tempColor(float c) {
  if (c < -5.0f)  return 0x64BF;   // svetla modra - silny mraz
  if (c <  5.0f)  return C_CYAN;
  if (c < 15.0f)  return C_GREEN;
  if (c < 25.0f)  return C_YELLOW;
  if (c < 30.0f)  return C_ORANGE;
  return C_RED;
}

// -----------------------------------------------------------------------------
//  Hodinove sloupce
// -----------------------------------------------------------------------------
static void drawHours() {
  const int n = Forecast_HourCount();
  const FcHour* h = Forecast_Hours();
  time_t rise = 0, set = 0;
  const bool haveSun = Forecast_SunTimes(&rise, &set);

  for (int i = 0; i < n && i < FORECAST_HOURS; i++) {
    const int cx = i * FC_HRS_W + FC_HRS_W / 2;

    struct tm lt;
    localtime_r(&h[i].t, &lt);
    char lab[8];
    snprintf(lab, sizeof(lab), "%dh", lt.tm_hour);
    textCenteredIfFree(lab, cx, FC_HRS_LABEL, C_GRAY, 1);

    // Ikona ma v noci mesic misto slunce. "Noc" se urcuje z casu TE hodiny,
    // ne z aktualniho stavu - sloupec pro tretí hodinu rano ma byt nocni,
    // i kdyz se na nej divate v poledne.
    bool night = false;
    if (haveSun) night = (h[i].t < rise || h[i].t >= set);
    WxIcon_Draw(cx, FC_HRS_ICON, 16, h[i].code, night);

    // Jednotka malym pismem za hodnotou. Znaminko stupne se nepouziva -
    // vestaveny font je 7bitove ASCII a "°" by vyslo jako nahodny znak,
    // takze je tu OUTSIDE_DEG_TEXT ("degC" nebo "\xB0C", viz Config.h).
    char t[8];
    snprintf(t, sizeof(t), "%d", (int)lroundf(h[i].temp));
    valueWithUnitCentered(cx, FC_HRS_TEMP, t, OUTSIDE_DEG_TEXT,
                          tempColor(h[i].temp), C_GRAY);

    if (h[i].precip > 0.05f) {
      char p[10];
      snprintf(p, sizeof(p), "%.1f", h[i].precip);
      valueWithUnitCentered(cx, FC_HRS_RAIN, p, "mm", C_CYAN, C_GRAY);
    }
  }
}

// -----------------------------------------------------------------------------
//  Denni radky
// -----------------------------------------------------------------------------
static void drawDays() {
  const int n = Forecast_DayCount();
  const FcDay* d = Forecast_Days();

  for (int i = 0; i < n && i < FORECAST_DAYS; i++) {
    const int y = FC_DAY_Y0 + i * FC_DAY_H;

    struct tm lt;
    localtime_r(&d[i].t, &lt);
    char name[12];
    snprintf(name, sizeof(name), "%s %d.", Lang_WeekdayShort(lt.tm_wday), lt.tm_mday);
    textIfFree(name, FC_D_NAME_X, y + 12, C_WHITE, 2);

    WxIcon_Draw(FC_D_ICON_X, y + 20, 18, d[i].code, false);

    // Maximum oranzove, minimum modre - dve cisla vedle sebe se tim rozlisi
    // i bez popisku.
    char tmax[8], tmin[8];
    snprintf(tmax, sizeof(tmax), "%d", (int)lroundf(d[i].tmax));
    snprintf(tmin, sizeof(tmin), "%d", (int)lroundf(d[i].tmin));
    // Maximum, lomitko, minimum a az za tim jednotka malym pismem - jedna
    // rezervace pres celou skupinu, jinak by se naroky prekryly (viz
    // poznamka u valueWithUnit). Obe cisla se barvi podle sve hodnoty
    // stejne jako hodinove teploty, takze barva znamena vsude totez;
    // ktere je maximum a ktere minimum, rika poradi.
    {
      const int wMax = Layout_TextW(tmax, 2), wSep = Layout_TextW("/", 2);
      const int wMin = Layout_TextW(tmin, 2), wU = Layout_TextW(OUTSIDE_DEG_TEXT, 1);
      const int total = wMax + 4 + wSep + 4 + wMin + 3 + wU;
      if (Layout_Claim(FC_D_TEMP_X - 2, y + 10, total + 4, LY_CHAR_H(2) + 4)) {
        int x = FC_D_TEMP_X;
        textRaw(tmax, x, y + 12, tempColor(d[i].tmax), 2); x += wMax + 4;
        textRaw("/",  x, y + 12, C_GRAY,   2); x += wSep + 4;
        textRaw(tmin, x, y + 12, tempColor(d[i].tmin), 2); x += wMin + 3;
        textRaw(OUTSIDE_DEG_TEXT, x, y + 20, C_GRAY, 1);
      }
    }

    if (d[i].precip > 0.05f) {
      char r[10];
      snprintf(r, sizeof(r), "%.1f", d[i].precip);
      valueWithUnit(FC_D_RAIN_X, y + 12, r, "mm", C_CYAN, C_GRAY);
    }
    {
      char w[10];
      snprintf(w, sizeof(w), "%.0f", d[i].wind);
      valueWithUnit(FC_D_WIND_X, y + 12, w, "km/h", C_LTGRAY, C_GRAY);
    }
  }
}

// -----------------------------------------------------------------------------
//  Radek ovzdusi
// -----------------------------------------------------------------------------
static const char* aqiWord(int aqi) {
  // Evropsky AQI: 0-20 velmi dobre, 20-40 dobre, 40-60 stredni,
  // 60-80 spatne, 80-100 velmi spatne, nad 100 extremne spatne.
  if (aqi < 0)   return "";
  if (aqi <= 20) return "velmi dobre";
  if (aqi <= 40) return "dobre";
  if (aqi <= 60) return "stredni";
  if (aqi <= 80) return "spatne";
  if (aqi <= 100) return "velmi spatne";
  return "extremne spatne";
}

static uint16_t aqiColor(int aqi) {
  if (aqi < 0)   return C_GRAY;
  if (aqi <= 20) return C_GREEN;
  if (aqi <= 40) return C_GREEN;
  if (aqi <= 60) return C_YELLOW;
  if (aqi <= 80) return C_ORANGE;
  return C_RED;
}

static void drawAirQuality() {
  if (!AirQuality_Valid()) return;

  // Tri skupiny vedle sebe: maly popisek nahore, velka hodnota pod nim.
  // Driv to byl jeden dlouhy radek velikosti 1, ve kterem se cisla ztracela.
  const int aqi = AirQuality_Aqi();

  if (aqi >= 0) {
    char v[8];
    snprintf(v, sizeof(v), "%d", aqi);
    labelledValue(16, FC_AQ_LABEL, FC_AQ_VALUE, "AQI", v, aqiWord(aqi),
                  aqiColor(aqi), C_LTGRAY);
  }

  if (AirQuality_Pm25() > 0) {
    char v[10];
    snprintf(v, sizeof(v), "%.0f", AirQuality_Pm25());
    // Open-Meteo vraci PM2.5 v mikrogramech na metr krychlovy. "ug" samotne
    // je nedokoncena jednotka - musi tam byt i vztazna velicina.
    labelledValue(200, FC_AQ_LABEL, FC_AQ_VALUE, "PM2.5", v, "ug/m3",
                  C_LTGRAY, C_GRAY);
  }

  // Pyl vraci API jen v Evrope a jen v sezone; mimo ni skupina proste chybi.
  // To je normalni stav, ne chyba.
  const float pollen = AirQuality_PollenMax();
  if (pollen >= 0 && AirQuality_PollenWorst()[0]) {
    // DRUH PYLU PATRI DO POPISKU, ne za hodnotu: druh neni jednotka, je to
    // informace o tom, CO se meri - presne jako "PM2.5" ve skupine vedle.
    //
    // Jednotka se ZAMERNE neuvadi. Pyl chodi v zrnkach na metr krychlovy,
    // jenze co je hodne a co malo se bez tabulky prahovych hodnot stejne
    // nepozna, takze "z/m3" jen zabiralo misto vedle nazvu druhu, ktery
    // je tou uzitecnou informaci.
    char label[20];
    snprintf(label, sizeof(label), "Pyl %s", AirQuality_PollenWorst());
    char v[10];
    snprintf(v, sizeof(v), "%.0f", pollen);
    labelledValue(330, FC_AQ_LABEL, FC_AQ_VALUE, label, v, nullptr,
                  C_YELLOW, C_GRAY);
  }
}

// -----------------------------------------------------------------------------
void ScreenForecast_Draw() {
  gfx->fillScreen(C_BLACK);

  Layout_Begin();
  Layout_Reserve(LY_DOTS_X, LY_DOTS_Y0, LY_DOTS_W, LY_DOTS_H);

  // Hlavicka
  gfx->setTextSize(2); gfx->setTextColor(C_CYAN);
  gfx->setCursor(10, FC_HDR_Y);
  gfx->print("Predpoved");
  {
    char st[OUTSIDE_TEXT_MAX];
    Outside_StatusText(st, sizeof(st));
    if (st[0]) {
      gfx->setTextSize(1); gfx->setTextColor(C_GRAY);
      gfx->setCursor(LCD_WIDTH - 10 - Layout_TextW(st, 1), FC_HDR_Y + 6);
      gfx->print(st);
    }
  }
  Layout_ReserveBand(FC_HDR_Y - 2, 24);

  if (!Forecast_Valid()) {
    UI_TextCentered(T(S_LOADING), LCD_HEIGHT / 2 - 10, C_GRAY, 2);
    UI_TextCentered("predpoved z Open-Meteo", LCD_HEIGHT / 2 + 18, C_GRAY, 1);
    return;
  }

  gfx->drawFastHLine(0, FC_SEP1_Y, LCD_WIDTH, C_DKGRAY);
  gfx->drawFastHLine(0, FC_SEP2_Y, LCD_WIDTH, C_DKGRAY);
  gfx->drawFastHLine(0, FC_SEP3_Y, LCD_WIDTH, C_DKGRAY);
  Layout_ReserveBand(FC_SEP1_Y - 1, 3);
  Layout_ReserveBand(FC_SEP2_Y - 1, 3);
  Layout_ReserveBand(FC_SEP3_Y - 1, 3);

  drawHours();
  drawDays();
  drawAirQuality();
}
