// =============================================================================
//  ESPD35_MeteoPlaneRadar
//  Vector weather icons drawn from a WMO code.
//
//  Vector rather than bitmaps on purpose: an icon set at three sizes would be
//  a few hundred kilobytes of flash and would still look wrong at a fourth
//  size. These are a handful of circles and lines, scale to any radius, and
//  cost nothing to store.
//
//  Prevzato z petus/MeteoPlaneRadar v0.6.1 (autor Petr / chiptron.cz)
//  a upraveno pro ESPD-3.5 (ILI9488 480x320, SPI).
// =============================================================================
#pragma once
#include <Arduino.h>

// Coarse groups - everything the drawing code actually distinguishes.
enum WxKind : uint8_t {
  WX_CLEAR = 0, WX_PARTLY, WX_CLOUDY, WX_FOG,
  WX_DRIZZLE, WX_RAIN, WX_SNOW, WX_SHOWER, WX_STORM
};

WxKind WxIcon_Kind(int wmoCode);

// Draw centred on (cx, cy) fitting inside a box of 2*r. night = draw a moon
// instead of a sun where the icon has one.
void WxIcon_Draw(int cx, int cy, int r, int wmoCode, bool night);

// A colour that suits the condition - used for the temperature next to it.
uint16_t WxIcon_Color(int wmoCode);
