// =============================================================================
//  ESPD35_MeteoPlaneRadar - obrazovka predpovedi. Viz ScreenForecast.h.
//
//  Rozvrzeni po radcich prevzate z petus/MeteoPlaneRadar 0.7.0 a nakreslene
//  pro obdelnik 480x320.
// =============================================================================
#include "Lang.h"     // LT(): cesky / slovensky, s diakritikou i bez
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
//
// Od teto verze se predpoved cte PO RADCICH, stejne jako v petus/MeteoPlaneRadar
// 0.7.0:
//
//     nyni / 12-15h / 15-18h   - zbytek odpoledne, popsany hodinami
//     dnes                     - jediny den, ke kteremu nikdo nepocita
//     Ct 6.11., Pa 7.11. ...   - dny v tydnu i s datem
//
// Drive to bylo sest po sobe jdoucich hodin a tri dny. To odpovidalo jen na
// "co udela pristich sest hodin" a nic jineho. Okno "12-15h" uz je odpoved,
// "za tri hodiny" je pocitani.
//
// SVISLE ROZESTUPY MUSI POCITAT S REZERVACI MISTA, ne jen s vyskou pisma:
// hodnota velikosti 2 narokuje LY_CHAR_H(2) + 4 = 20 px a zacina dva pixely
// nad textem. Radky jsou proto vysoke 26 a 27 px a cary mezi nimi lezi mimo
// naroky obou sousedu.
#define FC_SEP1_Y     30
#define FC_HOUR_Y0    34      // radek "nyni" a dve okna
#define FC_HOUR_H     26
#define FC_SEP2_Y    (FC_HOUR_Y0 + 3 * FC_HOUR_H + 4)        // 116
#define FC_DAY_Y0    (FC_SEP2_Y + 4)                         // 120
#define FC_DAY_H      27      // 5 x 27 = 135, konci na 255
#define FC_SEP3_Y    260

// Radek ovzdusi: maly popisek nahore, VELKA hodnota pod nim. Konci na 292,
// tecky prepinani obrazovek zacinaji na 298.
#define FC_AQ_LABEL  266
#define FC_AQ_VALUE  276

// Sloupce. Popisek drzi "St 31.12." velikosti 2 - devet znaku, 108 px, nejdelsi
// datum, ktere obrazovka kdy vyrobi. Zbytek je rozlozeny po sirce 480 px, ktera
// na rozdil od kulateho displeje nikde neubyva.
#define FC_COL_LABEL  10
#define FC_COL_ICON  140      // stred ikony
#define FC_COL_TEMP  168
#define FC_COL_RAIN  290
#define FC_COL_WIND  385

// Vitr pod touto hodnotou neni zprava a sloupec je lepsi nechat prazdny -
// stejne pravidlo, jakym se odjakziva ridi srazky.
#define WIND_SHOW_KMH 12.0f

static unsigned long s_lastDraw = 0;
static int           s_lastHour = -1;
static bool          s_lastValid = false;

void ScreenForecast_Enter() { s_lastDraw = 0; s_lastHour = -1; }

// Radky se meni jen s celou hodinou (posune se "nyni" a okna) a s novymi daty.
// Hodiny v hlavicce se meni kazdou minutu a kresli se se stejnym prekreslenim.
bool ScreenForecast_Tick() {
  bool want = false;
  const bool valid = Forecast_Valid();
  time_t now = time(nullptr);
  struct tm lt; localtime_r(&now, &lt);
  if (valid != s_lastValid || lt.tm_hour != s_lastHour) {
    s_lastValid = valid;
    s_lastHour = lt.tm_hour;
    want = true;
  }
  if (UI_StatusLineChanged()) want = true;

  // Pojistka: nova predpoved prijde kazdou pulhodinu a na jeji obsah se jinak
  // nic nepta. Jednou za minutu prekreslit nic nestoji.
  const unsigned long ms = millis();
  if (!s_lastDraw || ms - s_lastDraw >= 60000UL) want = true;
  if (want) s_lastDraw = ms ? ms : 1;
  return want;
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
//  Radky
// -----------------------------------------------------------------------------
static bool isNightAt(time_t t) {
  time_t rise = 0, set = 0;
  if (!Forecast_SunTimes(&rise, &set)) return false;
  // Casy slunce jsou dnesni; pro okno pres pulnoc staci porovnat cas dne.
  // Porovnava se cas dne v UTC. Kde zapad v UTC pripada az za pulnoc (Amerika),
  // je poradi obracene a noc je to, co lezi mezi zapadem a vychodem.
  const long day = 86400L;
  const long tt = (long)(t % day), rr = (long)(rise % day), ss = (long)(set % day);
  if (rr <= ss) return (tt < rr || tt >= ss);
  return (tt >= ss && tt < rr);
}

// Spolecny zbytek vsech radku: ikona, srazky, vitr. Teplotu si kresli radek
// sam, protoze u dnu jsou to dve cisla.
static void drawTail(int y, int code, bool night, float precip, float wind, int iconR) {
  if (Layout_Claim(FC_COL_ICON - iconR, y + 8 - iconR, 2 * iconR, 2 * iconR))
    WxIcon_Draw(FC_COL_ICON, y + 8, iconR, code, night);

  // Srazky jen kdyz nejake jsou - sloupec "0.0" je sum.
  if (precip >= 0.05f) {
    char p[10];
    snprintf(p, sizeof(p), "%.1f", precip);
    valueWithUnit(FC_COL_RAIN, y, p, "mm", C_CYAN, C_GRAY);
  }
  if (wind >= WIND_SHOW_KMH) {
    char w[10];
    snprintf(w, sizeof(w), "%d", (int)lroundf(wind));
    valueWithUnit(FC_COL_WIND, y, w, "km/h", C_LTGRAY, C_GRAY);
  }
}

static void drawSingleTemp(int y, float temp) {
  char t[8];
  snprintf(t, sizeof(t), "%d", (int)lroundf(temp));
  valueWithUnit(FC_COL_TEMP, y, t, OUTSIDE_DEG_TEXT, tempColor(temp), C_GRAY);
}

// --- "nyni" ------------------------------------------------------------------
// Prednost ma blok "current" pred hourly[0]: hodinova hodnota je modelove cislo
// za celou hodinu, takze ve 14:55 popisuje hodinu, ktera uz skoro skoncila.
static void drawNowRow(int rowY) {
  const FcHour* hrs = Forecast_Hours();
  if (Forecast_HourCount() < 1 && !Forecast_CurrentValid()) return;
  const int y = rowY + 5;

  float temp, precip, wind; int code;
  if (Forecast_CurrentValid()) {
    temp = Forecast_CurrentTemp(); precip = Forecast_CurrentPrecip();
    wind = Forecast_CurrentWind(); code = Forecast_CurrentCode();
  } else {
    temp = hrs[0].temp; precip = hrs[0].precip; wind = hrs[0].wind; code = hrs[0].code;
  }

  textIfFree(T(S_NOW), FC_COL_LABEL, y, C_WHITE, 2);
  drawSingleTemp(y, temp);
  drawTail(y, code, isNightAt(time(nullptr)), precip, wind, 10);
}

// --- Okno FORECAST_WIN_H hodin -----------------------------------------------
// Kazda hodnota se agreguje tak, jak se pouziva:
//   teplota - prumer ("bude kolem"),
//   srazky  - SOUCET (zalezi na tom, kolik spadne celkem),
//   vitr    - MAXIMUM (jedna narazova hodina zkazi cele okno),
//   ikona   - nejvyssi kod WMO, coz je skoro "nejhorsi z nich": stupnice jde
//             jasno, oblacno, mlha, mrholeni, dest, snih, prehanky, bourka.
static void drawWindowRow(int i0, int rowY) {
  const FcHour* hrs = Forecast_Hours();
  const int n = Forecast_HourCount();
  const int i1 = i0 + FORECAST_WIN_H - 1;
  if (i1 >= n) return;
  const int y = rowY + 5;

  float sumT = 0, sumP = 0, maxW = 0;
  int code = 0;
  for (int i = i0; i <= i1; i++) {
    sumT += hrs[i].temp;
    sumP += hrs[i].precip;
    if (hrs[i].wind > maxW) maxW = hrs[i].wind;
    if (hrs[i].code > code) code = hrs[i].code;
  }

  // Popisek od zacatku prvni hodiny do KONCE posledni, takze tri hodiny se
  // opravdu ctou jako tri: 12-15, ne 12-14. Okno koncici o pulnoci je 21-24.
  struct tm a, b;
  localtime_r(&hrs[i0].t, &a);
  const time_t endT = hrs[i1].t + 3600;
  localtime_r(&endT, &b);
  int endHour = b.tm_hour;
  if (endHour == 0) endHour = 24;

  char lbl[12];
  snprintf(lbl, sizeof(lbl), "%02d-%02dh", a.tm_hour, endHour);
  textIfFree(lbl, FC_COL_LABEL, y, C_GRAY, 2);
  drawSingleTemp(y, sumT / (float)FORECAST_WIN_H);
  drawTail(y, code, isNightAt(hrs[i0].t + (FORECAST_WIN_H * 3600) / 2), sumP, maxW, 10);
}

// --- Denni radek -------------------------------------------------------------
// "dnes" si nechava jmeno, protoze ho nikdo nepocita a datum nepotrebuje. Ostatni
// radky jsou den v tydnu a datum: "St 16.9." - den se cte, datum se kontroluje
// proti kalendari, a tyden dopredu chcete obe.
static void drawDayRow(const FcDay& d, int index, int rowY) {
  const int y = rowY + 5;
  struct tm lt; localtime_r(&d.t, &lt);

  char name[16];
  if (index == 0) snprintf(name, sizeof(name), "%s", T(S_TODAY));
  else snprintf(name, sizeof(name), "%s %d.%d.", Lang_WeekdayShort(lt.tm_wday),
                lt.tm_mday, lt.tm_mon + 1);
  textIfFree(name, FC_COL_LABEL, y, index == 0 ? C_WHITE : C_GRAY, 2);

  // Maximum, lomitko, minimum a za tim jednotka malym pismem - jedna rezervace
  // pres celou skupinu (viz poznamka u valueWithUnit). Obe cisla se barvi podle
  // sve hodnoty stejne jako teploty nahore; ktere je ktere, rika poradi.
  char tmax[8], tmin[8];
  snprintf(tmax, sizeof(tmax), "%d", (int)lroundf(d.tmax));
  snprintf(tmin, sizeof(tmin), "%d", (int)lroundf(d.tmin));
  const int wMax = Layout_TextW(tmax, 2), wSep = Layout_TextW("/", 2);
  const int wMin = Layout_TextW(tmin, 2), wU = Layout_TextW(OUTSIDE_DEG_TEXT, 1);
  const int total = wMax + 2 + wSep + 2 + wMin + 3 + wU;
  if (Layout_Claim(FC_COL_TEMP - 2, y - 2, total + 4, LY_CHAR_H(2) + 4)) {
    int x = FC_COL_TEMP;
    textRaw(tmax, x, y, tempColor(d.tmax), 2); x += wMax + 2;
    textRaw("/",  x, y, C_GRAY, 2);            x += wSep + 2;
    textRaw(tmin, x, y, tempColor(d.tmin), 2); x += wMin + 3;
    textRaw(OUTSIDE_DEG_TEXT, x, y + 8, C_GRAY, 1);
  }

  drawTail(y, d.code, false, d.precip, d.wind, 11);
}

// -----------------------------------------------------------------------------
//  Radek ovzdusi
// -----------------------------------------------------------------------------
static const char* aqiWord(int aqi) {
  // Evropsky AQI: 0-20 velmi dobre, 20-40 dobre, 40-60 stredni,
  // 60-80 spatne, 80-100 velmi spatne, nad 100 extremne spatne.
  if (aqi < 0)   return "";
  if (aqi <= 20) return LT("velmi dobre", "velmi dobré", "velmi dobre", "veľmi dobré");
  if (aqi <= 40) return LT("dobre", "dobré", "dobre", "dobré");
  if (aqi <= 60) return LT("stredni", "střední", "stredne", "stredné");
  if (aqi <= 80) return LT("spatne", "špatné", "zle", "zlé");
  if (aqi <= 100) return LT("velmi spatne", "velmi špatné", "velmi zle", "veľmi zlé");
  return LT("extremne spatne", "extrémně špatné", "extremne zle", "extrémne zlé");
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
    snprintf(label, sizeof(label), "%s %s", T(S_POLLEN), AirQuality_PollenWorst());
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

  UI_DrawHeader(T(S_FORECAST), C_CYAN);
  Layout_ReserveBand(UI_HDR_Y - 2, UI_HDR_H);

  if (!Forecast_Valid()) {
    UI_TextCentered(T(S_LOADING), LCD_HEIGHT / 2 - 10, C_GRAY, 2);
    UI_TextCentered(LT("predpoved z Open-Meteo", "předpověď z Open-Meteo", "predpoved z Open-Meteo", "predpoveď z Open-Meteo"), LCD_HEIGHT / 2 + 18, C_GRAY, 1);
    return;
  }

  gfx->drawFastHLine(0, FC_SEP1_Y, LCD_WIDTH, C_DKGRAY);
  gfx->drawFastHLine(0, FC_SEP2_Y, LCD_WIDTH, C_DKGRAY);
  gfx->drawFastHLine(0, FC_SEP3_Y, LCD_WIDTH, C_DKGRAY);
  Layout_ReserveBand(FC_SEP1_Y - 1, 3);
  Layout_ReserveBand(FC_SEP2_Y - 1, 3);
  Layout_ReserveBand(FC_SEP3_Y - 1, 3);

  // --- nyni a dve okna po FORECAST_WIN_H hodinach ---
  drawNowRow(FC_HOUR_Y0);
  drawWindowRow(1,                  FC_HOUR_Y0 + FC_HOUR_H);
  drawWindowRow(1 + FORECAST_WIN_H, FC_HOUR_Y0 + 2 * FC_HOUR_H);

  // --- dnes a dalsi dny ---
  // Pocet radku plyne z mista mezi carami, ne z konstanty - kdyby se nekdy
  // posunula, rozvrzeni zustane poctive misto preteceni do radku ovzdusi.
  const FcDay* d = Forecast_Days();
  int dn = Forecast_DayCount();
  const int fits = (FC_SEP3_Y - 4 - FC_DAY_Y0) / FC_DAY_H;
  if (dn > fits) dn = fits;
  if (dn > FORECAST_DAYS) dn = FORECAST_DAYS;
  for (int i = 0; i < dn; i++) drawDayRow(d[i], i, FC_DAY_Y0 + i * FC_DAY_H);

  drawAirQuality();
}
