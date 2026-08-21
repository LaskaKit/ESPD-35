// =============================================================================
//  ESPD35_MeteoPlaneRadar - kalendarni pomucka sdilena hodinami a snimky radaru.
//
//  Prevzato z petus/MeteoPlaneRadar v0.6.1 beze zmeny (ciste vypocty, zadna
//  vazba na hardware).
// =============================================================================
#pragma once
#include <Arduino.h>

// Pocet dnu od 1970-01-01 pro obcansky (proleptickou gregorianskou) datum.
// Algoritmus days_from_civil Howarda Hinnanta: presny, bez cyklu a bez
// zavislosti na knihovni podpore dat.
//
// Pouziva se k prevodu UTC data na epochu BEZ pouziti systemovych hodin - coz
// je tady podstatne, protoze popisky snimku meteoradaru musi sedet i driv, nez
// se hodiny vubec staci nastavit.
static inline long TimeUtil_DaysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (long)era * 146097 + (long)doe - 719468;
}

// UTC kalendar -> sekundy od epochy.
static inline time_t TimeUtil_UtcToEpoch(int Y, int Mo, int D,
                                         int hh, int mm, int ss) {
  return (time_t)TimeUtil_DaysFromCivil(Y, (unsigned)Mo, (unsigned)D) * 86400L
       + (long)hh * 3600L + (long)mm * 60L + ss;
}
