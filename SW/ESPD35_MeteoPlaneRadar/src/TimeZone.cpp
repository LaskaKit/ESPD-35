// =============================================================================
//  ESPD35_MeteoPlaneRadar - mistni casove pasmo. Viz TimeZone.h.
// =============================================================================
#include "TimeZone.h"
#include "Config.h"

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static char s_tz[40] = "";
static bool s_fromNet = false;

const char* TimeZone_Text()        { return s_tz; }
bool        TimeZone_FromNetwork() { return s_fromNet; }

static void applyTz(const char* tz) {
  strncpy(s_tz, tz, sizeof(s_tz) - 1);
  s_tz[sizeof(s_tz) - 1] = '\0';
  setenv("TZ", s_tz, 1);
  tzset();
}

void TimeZone_Begin() {
  s_fromNet = false;
  applyTz(TZ_INFO);
}

int TimeZone_Offset() {
  // Ptat se knihovny C, ne drzet vlastni kopii: se zkompilovanym pravidlem
  // zalezi odpoved na datu a jen knihovna vi, jestli je dnes letni cas.
  //
  // Primocare by to bylo tm_gmtoff, jenze to pole je rozsireni BSD/glibc
  // a newlib v ESP32 core ho nema - na PC se to prelozi, na desce ne. Proto
  // oklikou: tentyz okamzik rozlozit na mistni a UTC kalendar a odecist.
  const time_t now = time(nullptr);
  struct tm lt, gt;
  localtime_r(&now, &lt);
  gmtime_r(&now, &gt);

  // Rozdil dnu. tm_yday na Silvestra skoci o cely rok, takze tam rozhoduji
  // roky a vysledek se omezi na jediny den, o ktery se posun muze lisit.
  int days = lt.tm_yday - gt.tm_yday;
  if (lt.tm_year != gt.tm_year) days = (lt.tm_year > gt.tm_year) ? 1 : -1;
  else if (days >  1)          days = -1;
  else if (days < -1)          days =  1;

  return days * 86400
       + (lt.tm_hour - gt.tm_hour) * 3600
       + (lt.tm_min  - gt.tm_min)  * 60
       + (lt.tm_sec  - gt.tm_sec);
}

void TimeZone_SetOffset(int seconds) {
  // Skutecne posuny jsou od -12:00 do +14:00; cokoli jineho je chyba
  // parsovani a hodiny se kvuli ni hybat nesmi.
  if (seconds < -12 * 3600 || seconds > 14 * 3600) {
    Serial.printf("CAS: ignoruji nesmyslny posun %d s\n", seconds);
    return;
  }

  // Uz sedi? Pak nic nerikat a - hlavne - nechat zkompilovane pravidlo, aby
  // ceska deska mela prava data letniho casu misto zamrzleho posunu, ktery by
  // byl po kazde zmene casu par tydnu o hodinu vedle.
  if (TimeZone_Offset() == seconds) return;

  // POSIX pocita ZAPAD jako kladny, tedy obracene nez vsichni ostatni, takze
  // se znamenko otaci presne tady. UTC+2 v Praze je retezec "UTC-2:00",
  // UTC-5 v New Yorku "UTC+5:00".
  const int west = -seconds;
  const char sign = (west < 0) ? '-' : '+';
  const int mag = (west < 0) ? -west : west;

  char tz[24];
  snprintf(tz, sizeof(tz), "UTC%c%d:%02d", sign, mag / 3600, (mag % 3600) / 60);
  applyTz(tz);
  s_fromNet = true;
  Serial.printf("CAS: posun %+d s, TZ=\"%s\"\n", seconds, s_tz);
}
