// =============================================================================
//  ESPD35_MeteoPlaneRadar - platno s vyhlazenym textem.
//
//  V petus/MeteoPlaneRadar 0.8.5 posila kazdy print() do AAText_Print() vlastni
//  trida Canvas16. Tahle deska kresli do Arduino_Canvas z knihovny GFX, takze
//  stejny hacek dela tenka odvozena trida: prepise obe varianty write(), pres
//  ktere jde kazde print()/printf(), a se zvolenym vyhlazenym pismem posle cely
//  retezec do AAText_Print() misto skladani znaku 5x7 jeden po druhem.
//  Velikosti, pro ktere vyhlazene pismo neni (hodiny maji 8), a volba
//  "pixelove" jdou puvodni cestou.
//
//  Rozlozeni obrazovek se nemeni: vyhlazeny text ma stejne vysoka pismena,
//  stoji na stejne uctari a nikdy neni sirsi nez puvodni (viz AAFont.h).
// =============================================================================
#pragma once
#include <Arduino_GFX_Library.h>
#include "AAFont.h"

class AACanvas : public Arduino_Canvas {
public:
  AACanvas(int16_t w, int16_t h, Arduino_G* output)
    : Arduino_Canvas(w, h, output) {}

  size_t write(const uint8_t* buf, size_t n) override {
    if (n && smooth()) {
      size_t start = 0;
      for (size_t i = 0; i <= n; i++) {
        if (i == n || buf[i] == '\n') {
          if (i > start)
            cursor_x += AAText_Print(buf + start, i - start, cursor_x, cursor_y,
                                     textsize_x, textcolor, textbgcolor);
          if (i < n) { cursor_x = 0; cursor_y += 8 * textsize_y; }
          start = i + 1;
        }
      }
      return n;
    }
    return Print::write(buf, n);     // po znacich -> write(uint8_t) nize
  }

  size_t write(uint8_t c) override {
    if (smooth()) return write(&c, 1);
    return Arduino_Canvas::write(c);
  }

private:
  bool smooth() const {
    return AAText_Enabled() && textsize_x >= 1 && textsize_x <= 5 && textsize_x == textsize_y;
  }
};
