// =============================================================================
//  ESPD35_MeteoPlaneRadar - texty rozhrani. Viz Lang.h.
// =============================================================================
#include "Lang.h"
#include "AAFont.h"   // AAText_Enabled - vyhlazena pisma maji diakritiku
#include "Settings.h"
#include "Config.h"

// Vsechny tabulky se generuji z tehoz seznamu, takze se nemuzou rozejit.
static const char* const DISP[STR_COUNT] = {
#define X(id, disp, web, skDisp, skWeb) disp,
  STRINGS(X)
#undef X
};
static const char* const WEB[STR_COUNT] = {
#define X(id, disp, web, skDisp, skWeb) web,
  STRINGS(X)
#undef X
};
static const char* const SK_DISP[STR_COUNT] = {
#define X(id, disp, web, skDisp, skWeb) skDisp,
  STRINGS(X)
#undef X
};
static const char* const SK_WEB[STR_COUNT] = {
#define X(id, disp, web, skDisp, skWeb) skWeb,
  STRINGS(X)
#undef X
};

bool        Lang_IsSk()  { return Settings_Lang() == LANG_SK; }
const char* Lang_Code()  { return Lang_IsSk() ? "sk" : "cs"; }

const char* Lang_Pick(const char* csPlain, const char* csAcc,
                      const char* skPlain, const char* skAcc) {
  const bool aa = AAText_Enabled();
  if (Lang_IsSk()) return aa ? skAcc : skPlain;
  return aa ? csAcc : csPlain;
}

// S vyhlazenym pismem (0.8.5) umi displej diakritiku. Text pro displej je pak
// ten z webu - az na par retezcu, ktere jsou na webu delsi a na displej by se
// nevesly; ty maji vlastni prizvucnou verzi stejne delky.
static const char* accented(StrId id) {
  const bool sk = Lang_IsSk();
  switch (id) {
    case S_NO_LOCATION: return sk ? SK_DISP[id] : DISP[id];          // "Nastav polohu"
    case S_FW_UPDATE:   return sk ? "Aktualizácia FW" : DISP[id];
    case S_OLD_DATA:    return sk ? "bez spojenia, staré dáta" : "bez spojení, starší data";
    case S_FRAME_WIDE:  return sk ? "snímka moc široká" : "snímek moc široký";
    case S_NO_TOMORROW: return sk ? "zajtra ešte nie je" : WEB[id];
    default:            return sk ? SK_WEB[id] : WEB[id];
  }
}

const char* T(StrId id) {
  if (id >= STR_COUNT) return "";
  if (AAText_Enabled()) return accented(id);
  return Lang_IsSk() ? SK_DISP[id] : DISP[id];
}
const char* TW(StrId id) {
  if (id >= STR_COUNT) return "";
  return Lang_IsSk() ? SK_WEB[id] : WEB[id];
}

const char* Lang_WeekdayShort(int wday) {
  static const char* W[7]   = { "Ne", "Po", "Ut", "St", "Ct", "Pa", "So" };
  static const char* WA[7]  = { "Ne", "Po", "Út", "St", "Čt", "Pá", "So" };
  static const char* SW[7]  = { "Ne", "Po", "Ut", "St", "St", "Pi", "So" };
  static const char* SWA[7] = { "Ne", "Po", "Ut", "St", "Št", "Pi", "So" };
  if (wday < 0 || wday > 6) return "";
  const bool aa = AAText_Enabled();
  if (Lang_IsSk()) return aa ? SWA[wday] : SW[wday];
  return aa ? WA[wday] : W[wday];
}

const char* Lang_MonthName(int mon) {
  static const char* M[12] = { "ledna", "unora", "brezna", "dubna",
                               "kvetna", "cervna", "cervence", "srpna",
                               "zari", "rijna", "listopadu", "prosince" };
  static const char* MA[12] = { "ledna", "února", "března", "dubna",
                                "května", "června", "července", "srpna",
                                "září", "října", "listopadu", "prosince" };
  static const char* SM[12] = { "januara", "februara", "marca", "aprila",
                                "maja", "juna", "jula", "augusta",
                                "septembra", "oktobra", "novembra", "decembra" };
  static const char* SMA[12] = { "januára", "februára", "marca", "apríla",
                                 "mája", "júna", "júla", "augusta",
                                 "septembra", "októbra", "novembra", "decembra" };
  if (mon < 0 || mon > 11) return "";
  const bool aa = AAText_Enabled();
  if (Lang_IsSk()) return aa ? SMA[mon] : SM[mon];
  return aa ? MA[mon] : M[mon];
}
