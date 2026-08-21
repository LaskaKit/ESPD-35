// =============================================================================
//  ESPD35_MeteoPlaneRadar - rozvrzeni obrazovky. Viz Layout.h.
// =============================================================================
#include "Layout.h"
#include <string.h>

// Kolik obdelniku se drzi najednou. Nejvic jich potrebuje obrazovka letadel:
// sest pevnych prvku a zbytek jsou volaci znaky. Ikony letadel misto NEzabiraji
// (jen se ptaji, jestli je volne), takze sem padaji opravdu jen popisky - a
// popisku se na 480x320 citelne vejde par desitek.
//
// Pri prekroceni se dalsi naroky proste odmitnou. To je spravne chovani, ne
// chyba: plna obrazovka JE duvod dalsi popisek nekreslit.
#define LAYOUT_MAX 96

struct Rect { int16_t x, y, w, h; };

static Rect s_r[LAYOUT_MAX];
static int  s_n = 0;

void Layout_Begin() { s_n = 0; }
int  Layout_Count() { return s_n; }

int Layout_TextW(const char* s, uint8_t size) {
  if (!s) return 0;
  return (int)strlen(s) * LY_CHAR_W(size);
}

int Layout_ChordHalf(int y) {
  (void)y;                     // na obdelniku na vysce nezalezi
  return LCD_WIDTH / 2;
}

bool Layout_OnScreen(int x, int y, int w, int h) {
  return (x >= 0 && y >= 0 && x + w <= LCD_WIDTH && y + h <= LCD_HEIGHT);
}

static bool overlaps(const Rect& a, int x, int y, int w, int h) {
  if (a.x + a.w <= x || x + w <= a.x) return false;
  if (a.y + a.h <= y || y + h <= a.y) return false;
  return true;
}

bool Layout_IsFree(int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) return true;
  for (int i = 0; i < s_n; i++) if (overlaps(s_r[i], x, y, w, h)) return false;
  return true;
}

static void push(int x, int y, int w, int h) {
  if (s_n >= LAYOUT_MAX) return;
  if (w <= 0 || h <= 0) return;
  s_r[s_n].x = (int16_t)x;
  s_r[s_n].y = (int16_t)y;
  s_r[s_n].w = (int16_t)w;
  s_r[s_n].h = (int16_t)h;
  s_n++;
}

void Layout_Reserve(int x, int y, int w, int h) { push(x, y, w, h); }

void Layout_ReserveBand(int y, int h) { push(0, y, LCD_WIDTH, h); }

void Layout_ReserveTextCentered(const char* s, uint8_t size, int cx, int y) {
  const int w = Layout_TextW(s, size);
  push(cx - w / 2, y, w, LY_CHAR_H(size));
}

bool Layout_Claim(int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) return false;
  if (s_n >= LAYOUT_MAX) return false;         // plno = nekreslit
  if (!Layout_IsFree(x, y, w, h)) return false;
  push(x, y, w, h);
  return true;
}

// -----------------------------------------------------------------------------
//  Self-test pevnych pruhu
//
//  Kontroluje jen konstanty LY_*, tedy to, co je spolecne vsem obrazovkam.
//  Bezi jednou pri startu a nic nestoji - ale odhali preklep driv, nez se
//  projevi jako dva popisky pres sebe nekde na displeji.
// -----------------------------------------------------------------------------
void Layout_SelfTest() {
#if LAYOUT_DEBUG
  struct Band { const char* name; int x, y, w, h; };
  // Tecky obrazovek jsou zamerne v nejhorsim pripade (pet obrazovek), aby se
  // kolize odhalila uz ted, ne az pri rozsireni na peti obrazovkove UI.
  const Band B[] = {
    { "LY_STATUS",     LY_STATUS_X,     LY_STATUS,          LY_STATUS_W,     LY_CHAR_H(1) },
    { "LY_LEGEND",     LY_LEGEND_X,     LY_LEGEND,          LY_LEGEND_W,     LY_LEGEND_H  },
    { "LY_RANGE",      LY_RANGE_X,      LY_RANGE,           LY_RANGE_W,      LY_CHAR_H(1) },
    { "LY_RANGE_DOTS", LY_RANGE_DOTS_X, LY_RANGE_DOTS_Y0,   LY_RANGE_DOTS_W, LY_RANGE_DOTS_H },
    { "LY_DOTS(5)",    LY_DOTS_X,       LY_DOTS_Y0,        LY_DOTS_W,       LY_DOTS_H    },
    { "LY_FOOTER",     LY_FOOTER_X,     LY_FOOTER,         LY_FOOTER_W,     LY_CHAR_H(1) },
  };
  const int n = (int)(sizeof(B) / sizeof(B[0]));
  int problems = 0;

  Serial.println("Layout self-test:");
  for (int i = 0; i < n; i++) {
    if (!Layout_OnScreen(B[i].x, B[i].y, B[i].w, B[i].h)) {
      Serial.printf("  CHYBA: %s (%d,%d %dx%d) je mimo displej %dx%d\n",
                    B[i].name, B[i].x, B[i].y, B[i].w, B[i].h,
                    LCD_WIDTH, LCD_HEIGHT);
      problems++;
    }
    for (int j = i + 1; j < n; j++) {
      const Rect a = { (int16_t)B[i].x, (int16_t)B[i].y,
                       (int16_t)B[i].w, (int16_t)B[i].h };
      if (overlaps(a, B[j].x, B[j].y, B[j].w, B[j].h)) {
        Serial.printf("  PREKRYV: %s (x %d..%d, y %d..%d) a %s (x %d..%d, y %d..%d)\n",
                      B[i].name, B[i].x, B[i].x + B[i].w, B[i].y, B[i].y + B[i].h,
                      B[j].name, B[j].x, B[j].x + B[j].w, B[j].y, B[j].y + B[j].h);
        problems++;
      }
    }
  }
  Serial.printf("Layout self-test: %d nalezu\n", problems);
#endif
}
