// =============================================================================
//  ESPD35_MeteoPlaneRadar - texty rozhrani, cesky, ve dvou pravopisech.
//
//  Proc dva: vestaveny font Arduino_GFX je 7bitove ASCII, takze cokoli
//  kresleneho na displej musi byt bez diakritiky ("Predpoved"). Prohlizec
//  zadny takovy problem nema a dostane skutecny text ("Předpověď").
//  Kdyby to byly dva nezavisle seznamy, jeden se driv nebo pozdeji zaktualizuje
//  a druhy ne - proto je to jedna tabulka o dvou sloupcich.
//
//  Prepinani jazyku tenhle projekt nema (na rozdil od petus/MeteoPlaneRadar,
//  odkud je myslenka prevzata) - anglicky sloupec je zamerne vynechan.
// =============================================================================
//  PROC SE TENHLE SOUBOR NEJMENUJE Strings.h
//  ---------------------------------------
//  Jmenoval se tak a na Windows to NESLO PRELOZIT. Souborove systemy na
//  Windows i macOS nerozlisuji velikost pismen, Arduino prida slozku sketche
//  do include cesty pres -I, a `Arduino.h` vtahne `<string.h>`, ktery si
//  vtahne `<strings.h>`. Kompilator pak nasel NAS `Strings.h` misto toho
//  systemoveho a ohlasil "multiple definition of enum StrId" - hlaseni, ktere
//  o skutecne pricine nerika vubec nic.
//
//  Nazev tedy nesmi kolidovat se zadnou standardni hlavickou ani pri ignorovani
//  velikosti pismen. NEPREJMENOVAVAT zpet na Strings.h.
#pragma once
#include <Arduino.h>

// X(id, pro DISPLEJ - jen ASCII, pro WEB - UTF-8)
#define STRINGS(X) \
  X(S_WIFI_WAIT,     "Cekam na WiFi",      "Čekám na WiFi") \
  X(S_DOWNLOADING,   "Stahuji...",         "Stahuji...") \
  X(S_LOADING,       "Nacitam...",         "Načítám...") \
  X(S_ERROR,         "Chyba",              "Chyba") \
  X(S_NO_LOCATION,   "Nastav polohu",      "Nastavte polohu") \
  X(S_NOT_CONNECTED, "nepripojeno",        "nepřipojeno") \
  X(S_SETTINGS,      "Nastaveni",          "Nastavení") \
  X(S_BRIGHTNESS,    "Jas",                "Jas") \
  X(S_LOCATION,      "Poloha:",            "Poloha:") \
  X(S_TOP,           "Nahore",             "Nahoře") \
  X(S_UNITS_AVIA,    "Jednotky: letecke",  "Jednotky: letecké") \
  X(S_UNITS_METRIC,  "Jednotky: metricke", "Jednotky: metrické") \
  X(S_WIFI_LOC,      "WiFi / poloha",      "WiFi / poloha") \
  X(S_FW_UPDATE,     "Aktualizace FW",     "Aktualizace firmwaru") \
  X(S_AIRCRAFT,      "letadel",            "letadel") \
  X(S_NONE_IN_RANGE, "zadne",              "žádné") \
  X(S_IN_RANGE,      "v okruhu",           "v okruhu") \
  X(S_ALTITUDE,      "Vyska",              "Výška") \
  X(S_SPEED,         "Rychlost",           "Rychlost") \
  X(S_TRACK,         "Kurz",               "Kurz") \
  X(S_CLIMB,         "Stoupani",           "Stoupání") \
  X(S_TYPE,          "Typ",                "Typ") \
  X(S_SIGNAL_LOST,   "signal ztracen",     "signál ztracen") \
  X(S_UNKNOWN,       "neznamy",            "neznámý") \
  X(S_EMERGENCY,     "NOUZE",              "NOUZE") \
  X(S_HIJACK,        "UNOS",               "ÚNOS") \
  X(S_RADIO_FAIL,    "BEZ RADIA",          "BEZ RÁDIA") \
  X(S_METEORADAR,    "Meteoradar",         "Meteoradar") \
  X(S_NOW,           "nyni",               "nyní") \
  X(S_MIN,           "min",                "min") \
  X(S_OLD_DATA,      "bez spojeni, starsi data", "bez spojení, zobrazena starší data") \
  X(S_FRAME_WIDE,    "snimek moc siroky",  "snímek je moc široký")

enum StrId : uint16_t {
#define X(id, disp, web) id,
  STRINGS(X)
#undef X
  STR_COUNT
};

// Pro DISPLEJ - jen ASCII, bezpecne s vestavenym fontem.
const char* T(StrId id);

// Pro prohlizec / web - skutecne UTF-8 s diakritikou.
const char* TW(StrId id);

// Kalendarni nazvy. Obojí je ASCII - kresli se na displeji, do prohlizece se
// neposilaji. wday 0 = nedele (odpovida struct tm), mon 0 = leden.
const char* Lang_WeekdayShort(int wday);
const char* Lang_MonthName(int mon);
