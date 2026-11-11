// =============================================================================
//  ESPD35_MeteoPlaneRadar - sdilene UI utility (text, QR kod).
// =============================================================================
#include "UI.h"
#include "Layout.h"   // Layout_TextW - sirka podle skutecne vykresleneho pisma
#include "Outside.h"
#include <string.h>
#include <math.h>
#include <qrcode.h>          // knihovna "QRCode" (ricmoo) z Library Manageru

void UI_TextCentered(const char* text, int cy, uint16_t color, uint8_t size) {
  UI_TextCenteredIn(text, 0, LCD_WIDTH, cy, color, size);
}

void UI_TextCenteredIn(const char* text, int x, int w, int cy,
                       uint16_t color, uint8_t size) {
  // Sirka z Layout_TextW, ne z getTextBounds(): ta zna jen vestaveny font,
  // a vyhlazeny text (0.8.5) je obvykle uzsi - centroval by se doleva.
  const int tw = Layout_TextW(text, size);
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  gfx->setCursor(x + (w - tw) / 2, cy);
  gfx->print(text);
}

// Jedna radialni cara (rucicka, znacka vyberu) - vysec nulove sirky.
static void radialLine(int cx, int cy, int rIn, int rOut, float a, uint16_t color) {
  const float c = cosf(a), s = sinf(a);
  gfx->drawLine(cx + (int)lroundf(rIn * c), cy + (int)lroundf(rIn * s),
                cx + (int)lroundf(rOut * c), cy + (int)lroundf(rOut * s), color);
}

// Vysec nejvys ctvrt otacky, vyplneny po pixelech: projde se jen obdelnik,
// ktery vysec opisuje, a kazdy pixel se vezme, kdyz lezi v mezikruzi a mezi
// obema hranicnimi paprsky. Do kterych uhlu pixel patri, rozhoduji vektorove
// soucty, ne atan2 - na ESP32 je to o rad levnejsi.
//
// Prvni verze kreslila vysec hustymi radialnimi carami (jako petus/
// MeteoPlaneRadar). Na kulatem displeji 480 px to staci, ale na prstenci
// o polomeru kolem 110 px zustavaly mezi sousednimi carami cerne tecky, ktere
// zadny krok uhlu spolehlive neodstranil.
static void fillSectorQuarter(int cx, int cy, int rIn, int rOut,
                              float a0, float a1, uint16_t color) {
  const float u0x = cosf(a0), u0y = sinf(a0);
  const float u1x = cosf(a1), u1y = sinf(a1);

  // Ohraniceni: rohy vysece a kazdy svetovy smer (nasobek 90 st.), ktery lezi
  // uvnitr - tam ma oblouk extrem.
  float xs[8], ys[8];
  int n = 0;
  xs[n] = rIn * u0x;  ys[n++] = rIn * u0y;
  xs[n] = rOut * u0x; ys[n++] = rOut * u0y;
  xs[n] = rIn * u1x;  ys[n++] = rIn * u1y;
  xs[n] = rOut * u1x; ys[n++] = rOut * u1y;
  const float QUARTER_RAD = 1.57079633f;   // ne HALF_PI - to je makro z Arduino.h
  for (int k = -8; k <= 8 && n < 8; k++) {
    const float c = k * QUARTER_RAD;
    if (c > a0 && c < a1) { xs[n] = rOut * cosf(c); ys[n++] = rOut * sinf(c); }
  }
  float minX = xs[0], maxX = xs[0], minY = ys[0], maxY = ys[0];
  for (int i = 1; i < n; i++) {
    if (xs[i] < minX) minX = xs[i];
    if (xs[i] > maxX) maxX = xs[i];
    if (ys[i] < minY) minY = ys[i];
    if (ys[i] > maxY) maxY = ys[i];
  }
  const int x0 = (int)floorf(minX) - 1, x1 = (int)ceilf(maxX) + 1;
  const int y0 = (int)floorf(minY) - 1, y1 = (int)ceilf(maxY) + 1;

  const long rin2  = (long)rIn * rIn;
  const long rout2 = (long)rOut * rOut + rOut;      // +r ~ hrana na pul pixelu

  for (int dy = y0; dy <= y1; dy++) {
    const int py = cy + dy;
    if (py < 0 || py >= LCD_HEIGHT) continue;
    // Zacatek behu se drzi zvlast od priznaku: dx je vlevo od stredu zaporne,
    // takze -1 jako "zadny beh" by bylo platnou souradnici.
    bool inRun = false;
    int runStart = 0;
    for (int dx = x0; dx <= x1 + 1; dx++) {
      bool in = false;
      if (dx <= x1) {
        const long d2 = (long)dx * dx + (long)dy * dy;
        if (d2 >= rin2 && d2 <= rout2) {
          // Za paprskem a0 (po smeru) a pred paprskem a1. Osa y miri dolu,
          // takze "po smeru hodinovych rucicek" je kladny vektorovy soucin.
          const float c0 = u0x * dy - u0y * dx;
          const float c1 = dx * u1y - dy * u1x;
          in = (c0 >= -0.5f) && (c1 >= -0.5f);
        }
      }
      if (in && !inRun) { inRun = true; runStart = dx; }
      if (!in && inRun) {
        gfx->drawFastHLine(cx + runStart, py, dx - runStart, color);
        inRun = false;
      }
    }
  }
}

// Vyplneny vysec mezikruzi. Uhly v radianech, 0 = vpravo, po smeru hodinovych
// rucicek (osa y miri dolu). a0 == a1 nakresli jednu radialni caru.
void UI_FillRing(int cx, int cy, int rIn, int rOut, float a0, float a1,
                 uint16_t color) {
  if (rOut <= 0 || rIn > rOut) return;
  if (rIn < 0) rIn = 0;
  if (a1 < a0) { const float t = a0; a0 = a1; a1 = t; }

  // Velmi uzky vysec (rucicka, znacka) je cara - po pixelech by z nej nezbylo nic.
  if (a1 - a0 < 0.002f) { radialLine(cx, cy, rIn, rOut, a0, color); return; }

  // Po ctvrtotackach, aby test dvou paprsku platil (funguje do 180 st.).
  const float QUARTER = 1.57079633f;
  for (float b0 = a0; b0 < a1; b0 += QUARTER) {
    float b1 = b0 + QUARTER;
    if (b1 > a1) b1 = a1;
    fillSectorQuarter(cx, cy, rIn, rOut, b0, b1, color);
  }
}

bool UI_StatusLineChanged() {
  static char last[OUTSIDE_TEXT_MAX] = "";
  char txt[OUTSIDE_TEXT_MAX];
  Outside_StatusText(txt, sizeof(txt));
  if (strcmp(txt, last) == 0) return false;
  strncpy(last, txt, sizeof(last) - 1);
  last[sizeof(last) - 1] = '\0';
  return true;
}

void UI_DrawHeader(const char* title, uint16_t color) {
  gfx->setTextSize(2);
  gfx->setTextColor(color);
  gfx->setCursor(UI_HDR_X, UI_HDR_Y);
  gfx->print(title ? title : "");

  char st[OUTSIDE_TEXT_MAX];
  Outside_StatusText(st, sizeof(st));
  if (!st[0]) return;
  const int tw = Layout_TextW(st, 1);
  gfx->setTextSize(1);
  gfx->setTextColor(C_GRAY);
  gfx->setCursor(LCD_WIDTH - UI_HDR_X - (int)tw, UI_HDR_Y + 6);
  gfx->print(st);
}

void UI_DrawWifiQR(const char* ssid, const char* password, bool open,
                   int x, int y, int size_px) {
  // WiFi QR payload: WIFI:T:nopass;S:<ssid>;; nebo ...WPA;...P:<heslo>;;
  String payload = "WIFI:T:";
  payload += open ? "nopass" : "WPA";
  payload += ";S:"; payload += ssid; payload += ";";
  if (!open) { payload += "P:"; payload += password; payload += ";"; }
  payload += ";";

  uint8_t version = 3;
  if (payload.length() > 60) version = 5;
  if (payload.length() > 100) version = 7;

  QRCode qr;
  uint8_t buf[qrcode_getBufferSize(7)];
  if (qrcode_initText(&qr, buf, version, ECC_MEDIUM, payload.c_str()) != 0) return;

  int modules = qr.size;
  int scale = size_px / (modules + 2);
  if (scale < 1) return;
  int qrPix = (modules + 2) * scale;

  gfx->fillRect(x, y, qrPix, qrPix, C_WHITE);   // bily podklad vc. tiche zony
  int off = x + scale, offY = y + scale;
  for (int my = 0; my < modules; my++) {
    for (int mx = 0; mx < modules; mx++) {
      if (qrcode_getModule(&qr, mx, my)) {
        gfx->fillRect(off + mx * scale, offY + my * scale, scale, scale, C_BLACK);
      }
    }
  }
}

void UI_TextCenteredBg(const char* text, int cy, uint16_t color, uint16_t bg,
                       uint8_t size) {
  const int w = Layout_TextW(text, size);
  gfx->setTextSize(size);
  gfx->setTextColor(color, bg);      // dvouargumentova varianta = s podkladem
  gfx->setCursor((LCD_WIDTH - w) / 2, cy);
  gfx->print(text);
}
