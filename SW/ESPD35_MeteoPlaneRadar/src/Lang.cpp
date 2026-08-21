// =============================================================================
//  ESPD35_MeteoPlaneRadar - texty rozhrani. Viz Lang.h.
// =============================================================================
#include "Lang.h"

// Obe tabulky se generuji z tehoz seznamu, takze se nemuzou rozejit.
static const char* const DISP[STR_COUNT] = {
#define X(id, disp, web) disp,
  STRINGS(X)
#undef X
};

static const char* const WEB[STR_COUNT] = {
#define X(id, disp, web) web,
  STRINGS(X)
#undef X
};

const char* T(StrId id)  { return (id < STR_COUNT) ? DISP[id] : ""; }
const char* TW(StrId id) { return (id < STR_COUNT) ? WEB[id]  : ""; }

const char* Lang_WeekdayShort(int wday) {
  static const char* W[7] = { "Ne", "Po", "Ut", "St", "Ct", "Pa", "So" };
  if (wday < 0 || wday > 6) return "";
  return W[wday];
}

const char* Lang_MonthName(int mon) {
  static const char* M[12] = { "ledna", "unora", "brezna", "dubna",
                               "kvetna", "cervna", "cervence", "srpna",
                               "zari", "rijna", "listopadu", "prosince" };
  if (mon < 0 || mon > 11) return "";
  return M[mon];
}
