// =============================================================================
//  ESPD35_MeteoPlaneRadar - venkovni teplota a stavovy radek. Viz Outside.h.
// =============================================================================
#include "Outside.h"
#include "Clock.h"
#include "Config.h"
#include <time.h>
#include <math.h>
#include <stdio.h>

static bool  s_tempOk = false;
static float s_tempC  = 0.0f;

bool Outside_TimeValid() { return Clock_Valid(); }

void Outside_NoteHttpDate(const char* date) { Clock_NoteHttpDate(date); }

void Outside_NoteTemp(float degC) {
  // Model obcas vrati NaN nebo hodnotu mimo jakykoli smysl; lepsi je ukazat
  // starou teplotu nez nesmysl.
  if (!(degC > -90.0f && degC < 60.0f)) return;
  s_tempC  = degC;
  s_tempOk = true;
}

bool  Outside_TempValid() { return s_tempOk; }
float Outside_Temp()      { return s_tempC; }

void Outside_StatusText(char* buf, size_t cap) {
  if (!buf || cap == 0) return;
  buf[0] = '\0';

  char t[8] = "";
  if (Outside_TimeValid()) {
    time_t now = time(nullptr);
    struct tm lt;
    localtime_r(&now, &lt);
    snprintf(t, sizeof(t), "%02d:%02d", lt.tm_hour, lt.tm_min);
  }

  char d[12] = "";
  if (s_tempOk) snprintf(d, sizeof(d), "%d %s", (int)lroundf(s_tempC), OUTSIDE_DEG_TEXT);

  // Ukazuje se jen to, co je znamo. Prazdna pulka radku je poctivejsi nez
  // "--:--" nebo "0 degC", ktere vypadaji jako namerena hodnota.
  if (t[0] && d[0])      snprintf(buf, cap, "%s  %s", t, d);
  else if (t[0])         snprintf(buf, cap, "%s", t);
  else if (d[0])         snprintf(buf, cap, "%s", d);
}
