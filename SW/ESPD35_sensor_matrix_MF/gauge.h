#pragma once
#include <stdint.h>

struct Level {
  float upTo;          // horní mez pásma (exkluzivně)
  uint16_t color;
  const char* name;
};

struct Gauge {
  // konfigurace
  const char* label;
  const char* units;   // "C" se vykreslí jako °C
  float minV, maxV;
  uint8_t decimals;
  bool logScale;
  const Level* levels;
  uint8_t nLevels;

  // stav
  int16_t x, y;        // levý horní roh karty
  float value;         // poslední změřená hodnota, NAN = bez dat
  float pos;           // animovaná pozice 0..1
  int16_t drawnAngle;
  uint16_t drawnColor;
  char drawnText[12];
  const char* drawnStatus;
};
