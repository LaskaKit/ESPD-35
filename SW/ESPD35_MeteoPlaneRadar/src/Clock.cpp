// =============================================================================
//  ESPD35_MeteoPlaneRadar - cas z hlavicky HTTP "Date". Viz Clock.h.
// =============================================================================
#include "Clock.h"
#include "TimeUtil.h"
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <stdio.h>

// Hranice "hodiny uz nekdo nastavil": 2025-01-01 00:00:00 UTC. Cerstve
// nabootovany ESP32 zacina na 1970, takze cokoli nad touhle hodnotou znamena,
// ze cas prisel z NTP nebo od nas.
static const time_t CLOCK_SANE_EPOCH = 1735689600L;

static bool s_ok      = false;
static bool s_fromNtp = false;

bool Clock_Valid() {
  if (s_ok) return true;
  // NTP nas o uspechu neinformuje - pozna se jen podle toho, ze systemove
  // hodiny najednou ukazuji rozumnou hodnotu.
  if (time(nullptr) > CLOCK_SANE_EPOCH) {
    s_ok = true;
    s_fromNtp = true;
    Serial.println("Hodiny: nastaveny z NTP");
    return true;
  }
  return false;
}

const char* Clock_Source() {
  if (!s_ok) return "-";
  return s_fromNtp ? "NTP" : "HTTP Date";
}

static int monthFromName(const char* m) {
  static const char* N[12] = { "Jan","Feb","Mar","Apr","May","Jun",
                               "Jul","Aug","Sep","Oct","Nov","Dec" };
  for (int i = 0; i < 12; i++) if (strncmp(m, N[i], 3) == 0) return i + 1;
  return 0;
}

void Clock_NoteHttpDate(const char* date) {
  if (!date || !*date) return;

  // NTP ma prednost. Kdyz uz hodiny bezi, hlavicku jen zahodime - viz Clock.h.
  if (Clock_Valid()) return;

  // Preferovany tvar podle RFC 7231: "Sun, 09 Aug 2026 20:00:56 GMT".
  const char* p = strchr(date, ' ');
  if (!p) return;
  p++;

  int D = 0, Y = 0, hh = 0, mm = 0, ss = 0;
  char mon[4] = {0};
  if (sscanf(p, "%d %3s %d %d:%d:%d", &D, mon, &Y, &hh, &mm, &ss) != 6) return;

  int Mo = monthFromName(mon);
  if (Mo == 0 || D < 1 || D > 31 || Y < 2025 || hh > 23 || mm > 59 || ss > 60) return;

  time_t utc = TimeUtil_UtcToEpoch(Y, Mo, D, hh, mm, ss);
  if (utc <= CLOCK_SANE_EPOCH) return;

  struct timeval tv;
  tv.tv_sec  = utc;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);

  s_ok = true;
  s_fromNtp = false;
  struct tm lt;
  localtime_r(&utc, &lt);
  Serial.printf("Hodiny: nastaveny z hlavicky Date (%02d:%02d) - NTP neprosel\n",
                lt.tm_hour, lt.tm_min);
}
