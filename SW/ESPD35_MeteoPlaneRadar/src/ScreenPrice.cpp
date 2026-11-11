// =============================================================================
//  ESPD35_MeteoPlaneRadar - obrazovka: spotova cena elektriny. Viz ScreenPrice.h.
// =============================================================================
#include "Lang.h"     // LT(): cesky / slovensky, s diakritikou i bez
#include "ScreenPrice.h"
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

// --- Rozvrzeni 480x320 -------------------------------------------------------
// Prstenec vlevo, ciselne udaje vpravo. Na kulatem displeji je cela obrazovka
// jeden cifernik a cisla se musi vejit do disku uprostred; na obdelniku je
// vedle prstence volny pruh sirky skoro dvesta pixelu, a tam se cte lip.
#define CX       148
#define CY       170
#define R_OUT    116      // horni okraj 54 (pod hlavickou), dolni 286
#define R_IN      84
#define R_LABEL   68      // popisky 0 / 6 / 12 / 18 uvnitr prstence

// Pravy panel.
#define PX0      284      // levy okraj
#define PW       (LCD_WIDTH - PX0 - 6)
#define P_WHAT_Y  40      // "nyni" / "13:45" / "nejlevneji"
#define P_BIG_Y   62      // velke cislo (velikost 5 = 40 px)
#define P_UNIT_Y 108      // "Kc/kWh"
#define P_SEP_Y  130
#define P_MIN_Y  140
#define P_MAX_Y  164
#define P_NOTE_Y 196      // "burza" / "+500 Kc/MWh +DPH"
#define P_HINT_Y 226      // co udela klepnuti

// Jak dlouho zustava vybrana ctvrthodina vypsana, nez se displej vrati k cene
// prave ted. Dost na precteni a porovnani s druhou, dost malo na to, aby se
// deska nechana o samote sama vratila k uzitecnemu stavu.
#define SEL_HOLD_MS 12000UL

static uint8_t       s_day = 0;          // 0 = dnes, 1 = zitra
static int           s_sel = -1;         // klepnuta ctvrthodina, -1 = zadna
static unsigned long s_selAt = 0;
static int           s_lastSlot = -1;
static bool          s_lastValid = false;
static bool          s_lastTomorrow = false;

void ScreenPrice_Enter() {
  s_day = 0;
  s_sel = -1;
  s_lastSlot = -1;
}

// Zitrek jde ukazat, az existuje - aukce se uzavira kolem 14:00 a do te doby
// opravdu neni co kreslit.
static void setDay(uint8_t d) {
  if (d == 1 && !Energy_TomorrowValid()) d = 0;
  if (d == s_day) return;
  s_day = d;
  s_sel = -1;
}

void ScreenPrice_ChangeRange(int dir) {
  (void)dir;                       // dva stavy - kazdy smer prepina
  setDay(s_day ? 0 : 1);
}

bool ScreenPrice_Tick() {
  bool want = false;

  const bool valid = Energy_PriceValid();
  if (valid != s_lastValid) { s_lastValid = valid; want = true; }
  const bool tom = Energy_TomorrowValid();
  if (tom != s_lastTomorrow) { s_lastTomorrow = tom; want = true; }

  // Vyprseni vyberu je zmena na displeji - musi si rict o prekresleni.
  if (s_sel >= 0 && millis() - s_selAt >= SEL_HOLD_MS) { s_sel = -1; want = true; }

  // Prekreslit po CTVRTHODINE: tehdy se posune rucicka a hlavne se zmeni cena
  // uprostred.
  if (Outside_TimeValid()) {
    time_t now = time(nullptr);
    struct tm lt; localtime_r(&now, &lt);
    const int slot = Price_SlotOf(lt.tm_hour, lt.tm_min);
    if (slot != s_lastSlot) { s_lastSlot = slot; want = true; }
  }

  // ...jenze hodiny v hlavicce se meni kazdou minutu.
  if (UI_StatusLineChanged()) want = true;
  return want;
}

// --- Barvy -------------------------------------------------------------------
static uint16_t mix565(uint16_t a, uint16_t b, float t) {
  const int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  const int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  const int r = ar + (int)((br - ar) * t + 0.5f);
  const int g = ag + (int)((bg - ag) * t + 0.5f);
  const int bl = ab + (int)((bb - ab) * t + 0.5f);
  return (uint16_t)((r << 11) | (g << 5) | bl);
}

// Zelena -> zluta -> cervena pres rozsah DANEHO dne. Rozpeti nesmi klesnout
// pod podlahu - na plochem dni dela rozdil par haleru a roztahnout ho pres
// celou paletu by namalovalo dramaticky obraz niceho. A nad PRICE_SCALE_MAX je
// skala pribita, takze drahy den vypada drahy.
static uint16_t priceColor(float czkMwh, float lo, float hi) {
  if (isnan(czkMwh)) return C_DKGRAY;
  if (czkMwh < PRICE_NEGATIVE_BELOW) return C_CYAN;   // za odber se plati

  float span = hi - lo;
  if (span < 400.0f) span = 400.0f;
  float t = (czkMwh - lo) / span;
  if (czkMwh >= PRICE_SCALE_MAX) t = 1.0f;
  if (t < 0) t = 0;
  if (t > 1) t = 1;

  if (t < 0.5f) return mix565(C_GREEN,  C_YELLOW, t * 2.0f);
  return               mix565(C_YELLOW, C_RED,    (t - 0.5f) * 2.0f);
}

// Pulnoc nahore, po smeru hodinovych rucicek. Jednotka je CTVRTHODINA, cela
// otacka je 96 kroku po 3,75 stupne.
static inline float slotAngle(float slot) {
  return (slot * (360.0f / PRICE_SLOTS) - 90.0f) * 0.0174532925f;
}

static float priceAt(int slot) {
  return s_day ? Energy_PriceTomorrow(slot) : Energy_PriceToday(slot);
}

// Kc/kWh s desetinnou carkou - tak se cislo v cestine pise a tenhle retezec
// nikdo neparsuje.
static void fmtKwh(float czkMwh, char* out, size_t cap) {
  if (isnan(czkMwh)) { snprintf(out, cap, "--"); return; }
  snprintf(out, cap, "%.2f", Energy_FinalCzkKwh(czkMwh));
  for (char* p = out; *p; p++) if (*p == '.') *p = ',';
}

// --- Prstenec ---------------------------------------------------------------
static void drawRing() {
  float lo = 0, hi = 0;
  if (!Energy_DayRange(s_day, &lo, &hi)) return;

  // Mezi ctvrthodinami zadna mezera: pri 3,75 stupne by snedla tretinu bloku
  // a cena behem dne je krivka, ne devadesat sest nesouvisejicich cisel.
  // Strukturu davaji ryzky po hodinach nize.
  for (int i = 0; i < PRICE_SLOTS; i++) {
    const float p = priceAt(i);
    if (isnan(p)) continue;
    UI_FillRing(CX, CY, R_IN, R_OUT, slotAngle(i), slotAngle(i + 1), priceColor(p, lo, hi));
  }

  // Kratky cerny zarez na vnejsim okraji kazde cele hodiny - dik nemu jde
  // dopocitat k hodine, ktera vas zajima, a barevny pas se nerozpadne.
  for (int h = 0; h < 24; h++) {
    const float a = slotAngle(h * PRICE_PER_HOUR);
    const float c = cosf(a), s = sinf(a);
    gfx->drawLine(CX + (int)((R_OUT - 7) * c), CY + (int)((R_OUT - 7) * s),
                  CX + (int)((R_OUT + 1) * c), CY + (int)((R_OUT + 1) * s), C_BLACK);
  }

  // Ctvrtiny dne pres celou sirku prstence a popisky uvnitr.
  for (int h = 0; h < 24; h += 6) {
    const float a = slotAngle(h * PRICE_PER_HOUR);
    const float c = cosf(a), s = sinf(a);
    gfx->drawLine(CX + (int)((R_IN - 4) * c), CY + (int)((R_IN - 4) * s),
                  CX + (int)((R_OUT + 4) * c), CY + (int)((R_OUT + 4) * s), C_BLACK);
    char lbl[4];
    snprintf(lbl, sizeof(lbl), "%d", h);
    const int w = Layout_TextW(lbl, 2);
    const int lx = CX + (int)(R_LABEL * c) - w / 2;
    const int ly = CY + (int)(R_LABEL * s) - 8;
    if (Layout_Claim(lx - 3, ly - 3, w + 6, LY_CHAR_H(2) + 6)) {
      gfx->setTextSize(2);
      gfx->setTextColor(C_GRAY);
      gfx->setCursor(lx, ly);
      gfx->print(lbl);
    }
  }
}

// Rucicka. Jen na dnesnim cifernicku - na zitrejsim by ukazovala na hodinu,
// ktera jeste nebyla, a to je horsi nez zadna znacka.
static void drawNeedle() {
  if (s_day != 0 || !Outside_TimeValid()) return;
  time_t now = time(nullptr);
  struct tm lt; localtime_r(&now, &lt);
  const float a = slotAngle((lt.tm_hour + lt.tm_min / 60.0f) * PRICE_PER_HOUR);

  // Tri cary kousek od sebe misto jedne: jednopixelova rucicka nad svetlym
  // vysekem je z dalky skoro neviditelna.
  for (int k = -1; k <= 1; k++)
    UI_FillRing(CX, CY, R_IN - 10, R_OUT + 8, a + k * 0.004f, a + k * 0.004f, C_WHITE);

  const float c = cosf(a), s = sinf(a);
  gfx->fillCircle(CX + (int)((R_OUT + 10) * c), CY + (int)((R_OUT + 10) * s), 4, C_WHITE);
}

// Klepnuta ctvrthodina, oznacena tak, aby bylo jasne, kterou panel vypisuje.
static void drawSelection() {
  if (s_sel < 0) return;
  const float a0 = slotAngle(s_sel), a1 = slotAngle(s_sel + 1);
  UI_FillRing(CX, CY, R_IN - 8, R_OUT + 8, a0, a0, C_WHITE);
  UI_FillRing(CX, CY, R_IN - 8, R_OUT + 8, a1, a1, C_WHITE);
}

// Stred prstence: ktery den se ukazuje, a jeho datum.
static void drawRingCentre() {
  const char* dayLbl = s_day ? T(S_TOMORROW) : T(S_TODAY);
  Layout_ReserveTextCentered(dayLbl, 3, CX, CY - 18);
  UI_TextCenteredIn(dayLbl, CX - R_IN, 2 * R_IN, CY - 18, C_WHITE, 3);

  if (Outside_TimeValid()) {
    time_t t = time(nullptr) + (s_day ? 86400 : 0);
    struct tm lt; localtime_r(&t, &lt);
    char d[16];
    snprintf(d, sizeof(d), "%s %d.%d.", Lang_WeekdayShort(lt.tm_wday), lt.tm_mday, lt.tm_mon + 1);
    Layout_ReserveTextCentered(d, 2, CX, CY + 12);
    UI_TextCenteredIn(d, CX - R_IN, 2 * R_IN, CY + 12, C_GRAY, 2);
  }
}

// --- Pravy panel -------------------------------------------------------------
// "min 03:15  1,23" - popisek sede, cas a hodnota barevne.
static void infoRow(int y, const char* label, int slot, uint16_t col) {
  char val[16], buf[32];
  fmtKwh(priceAt(slot), val, sizeof(val));
  snprintf(buf, sizeof(buf), "%02d:%02d %s",
           Price_SlotHour(slot), Price_SlotMinute(slot), val);
  // Bez Layout_Claim: pravy panel si rezervuje ScreenPrice_Draw() jako celek
  // a jeho radky maji pevne misto. Narok uvnitr vlastni rezervace by vzdy
  // selhal a radek by se tise nevykreslil.
  const int lw = Layout_TextW(label, 2);
  gfx->setTextSize(2);
  gfx->setTextColor(C_GRAY);
  gfx->setCursor(PX0, y);
  gfx->print(label);
  gfx->setTextColor(col);
  gfx->setCursor(PX0 + lw + 10, y);
  gfx->print(buf);
}

static void drawPanel() {
  char buf[40], val[24];

  // --- Co je velke cislo ---
  // Bud klepnuta ctvrthodina, nebo - na dnesku - ta, ve ktere jsme. Na zitrku
  // bez vyberu zadne "ted" neni, takze dostane misto nejlevnejsi blok: kvuli
  // nemu se na zitrek kdokoli diva.
  int showSlot = s_sel;
  bool isNow = false;
  if (showSlot < 0) {
    if (s_day == 0 && Outside_TimeValid()) {
      time_t now = time(nullptr);
      struct tm lt; localtime_r(&now, &lt);
      showSlot = Price_SlotOf(lt.tm_hour, lt.tm_min);
      isNow = true;
    } else {
      showSlot = Energy_CheapestSlot(s_day);
    }
  }

  if (isNow) snprintf(buf, sizeof(buf), "%s", T(S_NOW));
  else if (showSlot >= 0 && s_sel < 0)
    snprintf(buf, sizeof(buf), "min %02d:%02d", Price_SlotHour(showSlot), Price_SlotMinute(showSlot));
  else if (showSlot >= 0)
    snprintf(buf, sizeof(buf), "%02d:%02d-%02d:%02d",
             Price_SlotHour(showSlot), Price_SlotMinute(showSlot),
             Price_SlotHour(showSlot + 1) % 24, Price_SlotMinute(showSlot + 1));
  else buf[0] = '\0';
  if (buf[0]) {
    gfx->setTextSize(2); gfx->setTextColor(C_GRAY);
    gfx->setCursor(PX0, P_WHAT_Y); gfx->print(buf);
  }

  const float p = (showSlot >= 0) ? priceAt(showSlot) : NAN;
  fmtKwh(p, val, sizeof(val));
  float lo = 0, hi = 0;
  const bool haveRange = Energy_DayRange(s_day, &lo, &hi);
  const uint16_t col = haveRange ? priceColor(p, lo, hi) : C_WHITE;
  // Velikost 5 = 30 px na znak; "-1,23" i "12,34" se vejdou do 150 px.
  const uint8_t bigSz = (Layout_TextW(val, 5) <= PW) ? 5 : 4;
  gfx->setTextSize(bigSz); gfx->setTextColor(col);
  gfx->setCursor(PX0, P_BIG_Y); gfx->print(val);

  gfx->setTextSize(2); gfx->setTextColor(C_GRAY);
  gfx->setCursor(PX0, P_UNIT_Y); gfx->print(LT("Kc/kWh", "Kč/kWh", "Kc/kWh", "Kč/kWh"));

  gfx->drawFastHLine(PX0, P_SEP_Y, PW, C_DKGRAY);

  // --- Nejlevnejsi a nejdrazsi blok dne ---
  // Cas se sklada z Price_SlotHour/Minute, NE z cisla bloku: od prechodu na
  // ctvrthodiny je blok 0..95 a hole "%02d:00" tisklo veci jako "63:00".
  const int cheap = Energy_CheapestSlot(s_day);
  const int dear  = Energy_PriceHighestSlot(s_day);
  if (cheap >= 0) infoRow(P_MIN_Y, "min", cheap, C_GREEN);
  if (dear >= 0 && dear != cheap) infoRow(P_MAX_Y, "max", dear, C_ORANGE);

  // --- Co to cislo vlastne je ---
  // Cista burzovni cena a cena s vlastnim tarifem jsou velmi ruzna cisla a ktere
  // je na displeji, musi byt jednoznacne - jinak to nekdo porovna s fakturou
  // a usoudi, ze je deska rozbita. Proto sede a velikosti 2, ne tmave sedou
  // jednickou: vyhrada, kterou nejde precist, neni vyhrada.
  if (Settings_PriceFee() == 0 && Settings_PriceVat() == 0)
    snprintf(buf, sizeof(buf), "%s", T(S_SPOT));
  else
    snprintf(buf, sizeof(buf), "+%u Kc/MWh%s", (unsigned)Settings_PriceFee(),
             Settings_PriceVat() ? " +DPH" : "");
  {
    const uint8_t sz = (Layout_TextW(buf, 2) <= PW) ? 2 : 1;
    gfx->setTextSize(sz); gfx->setTextColor(C_GRAY);
    gfx->setCursor(PX0, P_NOTE_Y); gfx->print(buf);
  }

  // --- Co udela klepnuti, nebo proc neudela nic ---
  const char* hint = nullptr;
  if (Energy_TomorrowValid()) hint = s_day ? T(S_TODAY) : T(S_TOMORROW);
  else if (s_day == 0)        hint = T(S_NO_TOMORROW);
  if (hint) {
    if (Energy_TomorrowValid()) snprintf(buf, sizeof(buf), "%s: %s", LT("klepni", "klepni", "tukni", "ťukni"), hint);
    else                        snprintf(buf, sizeof(buf), "%s", hint);
    gfx->setTextSize(1); gfx->setTextColor(C_GRAY);
    gfx->setCursor(PX0, P_HINT_Y); gfx->print(buf);
  }
}

void ScreenPrice_Draw() {
  gfx->fillScreen(C_BLACK);
  Layout_Begin();
  Layout_Reserve(LY_DOTS_X, LY_DOTS_Y0, LY_DOTS_W, LY_DOTS_H);

  UI_DrawHeader(T(S_PRICE), C_CYAN);
  Layout_ReserveBand(UI_HDR_Y - 2, UI_HDR_H);

  // Mimo CR neni co nacitat a nikdy nic neprijde, takze to rict, misto
  // vecneho "Nacitam...". Stahovani take mlci - viz Energy_Tick().
  if (!Energy_PriceInArea()) {
    UI_TextCentered(T(S_PRICE_CZ_ONLY), LCD_HEIGHT / 2 - 10, C_YELLOW, 2);
    UI_TextCentered(T(S_OUT_OF_AREA), LCD_HEIGHT / 2 + 18, C_GRAY, 1);
    return;
  }
  if (!Energy_PriceValid()) {
    UI_TextCentered(WiFi.status() == WL_CONNECTED ? T(S_LOADING) : T(S_WIFI_WAIT),
                    LCD_HEIGHT / 2 - 10, C_YELLOW, 2);
    UI_TextCentered("spotovaelektrina.cz", LCD_HEIGHT / 2 + 18, C_GRAY, 1);
    return;
  }

  // Chtel se zitrek a mezitim se prehoupl den nebo data zmizela.
  if (s_day == 1 && !Energy_TomorrowValid()) s_day = 0;

  // Pravy panel si misto rezervuje driv, nez prstenec zacne narokovat popisky.
  Layout_Reserve(PX0 - 4, UI_HDR_H + 4, LCD_WIDTH - PX0 + 4, 240);

  drawRing();
  drawNeedle();
  drawSelection();
  drawRingCentre();
  drawPanel();
}

bool ScreenPrice_HandleTap(int x, int y) {
  if (!Energy_PriceValid()) return false;

  // Pravy panel: prepnuti dne, stejne jako stred prstence.
  if (x >= PX0 - 10) {
    if (!Energy_TomorrowValid()) {
      if (s_sel < 0) return false;
      s_sel = -1;
      return true;
    }
    setDay(s_day ? 0 : 1);
    return true;
  }

  const float dx = (float)(x - CX), dy = (float)(y - CY);
  const float r = sqrtf(dx * dx + dy * dy);

  // Uvnitr prstence: prepinac dne. Velky cil, protoze je to gesto, ktere
  // lide najdou jako prvni.
  if (r < R_IN - 6) {
    if (!Energy_TomorrowValid()) {
      // Neni na co prepnout. Zrusit vyber je i tak rozumna reakce.
      if (s_sel < 0) return false;
      s_sel = -1;
      return true;
    }
    setDay(s_day ? 0 : 1);
    return true;
  }

  // Na prstenci: vypsat tu ctvrthodinu. atan2f dava -pi..pi od osy +x a
  // cifernik zacina nahore, takze se pricte ctvrt otacky.
  if (r <= R_OUT + 14) {
    float a = atan2f(dy, dx) * 57.2957795f + 90.0f;
    while (a < 0)    a += 360.0f;
    while (a >= 360) a -= 360.0f;
    const int i = (int)(a / (360.0f / PRICE_SLOTS)) % PRICE_SLOTS;
    if (isnan(priceAt(i))) return false;
    s_sel = (s_sel == i) ? -1 : i;      // druhe klepnuti na tentyz blok ho zrusi
    s_selAt = millis();
    return true;
  }
  return false;
}
