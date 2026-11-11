// =============================================================================
//  ESPD35_MeteoPlaneRadar - texty rozhrani, cesky a slovensky, kazdy ve dvou
//  pravopisech.
//
//  Proc dva: vestaveny font Arduino_GFX je 7bitove ASCII, takze cokoli
//  kresleneho na displej musi byt bez diakritiky ("Predpoved"). Prohlizec
//  zadny takovy problem nema a dostane skutecny text ("Předpověď").
//  Kdyby to byly dva nezavisle seznamy, jeden se driv nebo pozdeji zaktualizuje
//  a druhy ne - proto je to jedna tabulka o dvou sloupcich.
//
//  Jazyk se voli na webu (Settings_Lang): cestina, nebo slovencina. Prepina
//  displej i webovou stranku; stranka si texty preklada sama v JavaScriptu.
//  Texty psane primo v kodu (ne v teto tabulce) jdou pres makro LT().
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

// X(id, cesky DISPLEJ - jen ASCII, cesky WEB - UTF-8, slovensky DISPLEJ, slovensky WEB)
#define STRINGS(X) \
  X(S_WIFI_WAIT, "Cekam na WiFi", "Čekám na WiFi", "Cakam na WiFi", "Čakám na WiFi") \
  X(S_DOWNLOADING, "Stahuji...", "Stahuji...", "Stahujem...", "Sťahujem...") \
  X(S_LOADING, "Nacitam...", "Načítám...", "Nacitavam...", "Načítavam...") \
  X(S_ERROR, "Chyba", "Chyba", "Chyba", "Chyba") \
  X(S_NO_LOCATION, "Nastav polohu", "Nastavte polohu", "Nastav polohu", "Nastavte polohu") \
  X(S_NOT_CONNECTED, "nepripojeno", "nepřipojeno", "nepripojene", "nepripojené") \
  X(S_SETTINGS, "Nastaveni", "Nastavení", "Nastavenia", "Nastavenia") \
  X(S_BRIGHTNESS, "Jas", "Jas", "Jas", "Jas") \
  X(S_LOCATION, "Poloha:", "Poloha:", "Poloha:", "Poloha:") \
  X(S_TOP, "Nahore", "Nahoře", "Hore", "Hore") \
  X(S_UNITS_AVIA, "Jednotky: letecke", "Jednotky: letecké", "Jednotky: letecke", "Jednotky: letecké") \
  X(S_UNITS_METRIC, "Jednotky: metricke", "Jednotky: metrické", "Jednotky: metricke", "Jednotky: metrické") \
  X(S_WIFI_LOC, "WiFi / poloha", "WiFi / poloha", "WiFi / poloha", "WiFi / poloha") \
  X(S_FW_UPDATE, "Aktualizace FW", "Aktualizace firmwaru", "Aktualizacia FW", "Aktualizácia firmvéru") \
  X(S_AIRCRAFT, "letadel", "letadel", "lietadiel", "lietadiel") \
  X(S_NONE_IN_RANGE, "zadne", "žádné", "ziadne", "žiadne") \
  X(S_IN_RANGE, "v okruhu", "v okruhu", "v okruhu", "v okruhu") \
  X(S_ALTITUDE, "Vyska", "Výška", "Vyska", "Výška") \
  X(S_SPEED, "Rychlost", "Rychlost", "Rychlost", "Rýchlosť") \
  X(S_TRACK, "Kurz", "Kurz", "Kurz", "Kurz") \
  X(S_CLIMB, "Stoupani", "Stoupání", "Stupanie", "Stúpanie") \
  X(S_TYPE, "Typ", "Typ", "Typ", "Typ") \
  X(S_SIGNAL_LOST, "signal ztracen", "signál ztracen", "signal strateny", "signál stratený") \
  X(S_UNKNOWN, "neznamy", "neznámý", "neznamy", "neznámy") \
  X(S_EMERGENCY, "NOUZE", "NOUZE", "NUDZA", "NÚDZA") \
  X(S_HIJACK, "UNOS", "ÚNOS", "UNOS", "ÚNOS") \
  X(S_RADIO_FAIL, "BEZ RADIA", "BEZ RÁDIA", "BEZ RADIA", "BEZ RÁDIA") \
  X(S_METEORADAR, "Meteoradar", "Meteoradar", "Meteoradar", "Meteoradar") \
  X(S_NOW, "nyni", "nyní", "teraz", "teraz") \
  X(S_MIN, "min", "min", "min", "min") \
  X(S_OLD_DATA, "bez spojeni, starsi data", "bez spojení, zobrazena starší data", "bez spojenia, stare data", "bez spojenia, zobrazené staršie dáta") \
  X(S_FRAME_WIDE, "snimek moc siroky", "snímek je moc široký", "snimka moc siroka", "snímka je príliš široká") \
  X(S_LOADING_NEWER, "nacitam novejsi snimky...", "načítám novější snímky...", "nacitavam novsie snimky...", "načítavam novšie snímky...") \
  X(S_FORECAST, "Predpoved", "Předpověď", "Predpoved", "Predpoveď") \
  X(S_POLLEN, "Pyl", "Pyl", "Pel", "Peľ") \
  X(S_TODAY, "dnes", "dnes", "dnes", "dnes") \
  X(S_TOMORROW, "zitra", "zítra", "zajtra", "zajtra") \
  X(S_PRICE, "Cena elektriny", "Cena elektřiny", "Cena elektriny", "Cena elektriny") \
  X(S_NO_TOMORROW, "zitrek zatim neni", "zítřek zatím není", "zajtra este nie je", "zajtrajšok ešte nie je") \
  X(S_SPOT, "burza", "burza", "burza", "burza") \
  X(S_DATA_FROM, "data z", "data z", "data z", "dáta z") \
  X(S_PRICE_CZ_ONLY, "jen pro CR", "jen pro ČR", "len pre CR", "len pre ČR") \
  X(S_MIX_EU_ONLY, "jen pro Evropu", "jen pro Evropu", "len pre Europu", "len pre Európu") \
  X(S_OUT_OF_AREA, "mimo oblast zdroje", "mimo oblast zdroje", "mimo oblasti zdroja", "mimo oblasti zdroja") \
  X(S_MIX, "Vyroba CR", "Výroba ČR", "Vyroba CR", "Výroba ČR") \
  X(S_MIX_GEN, "Vyroba", "Výroba", "Vyroba", "Výroba") \
  X(S_RENEWABLE, "OZE", "OZE", "OZE", "OZE") \
  X(S_CONSUMPTION, "spotreba", "spotřeba", "spotreba", "spotreba") \
  X(S_EXPORT, "vyvoz", "vývoz", "vyvoz", "vývoz") \
  X(S_IMPORT, "dovoz", "dovoz", "dovoz", "dovoz") \
  X(S_NUCLEAR, "jadro", "jádro", "jadro", "jadro") \
  X(S_COAL, "uhli", "uhlí", "uhlie", "uhlie") \
  X(S_GAS, "plyn", "plyn", "plyn", "plyn") \
  X(S_SOLAR, "slunce", "slunce", "slnko", "slnko") \
  X(S_WIND, "vitr", "vítr", "vietor", "vietor") \
  X(S_HYDRO, "voda", "voda", "voda", "voda") \
  X(S_BIOMASS, "biomasa", "biomasa", "biomasa", "biomasa") \
  X(S_OTHER, "ostatni", "ostatní", "ostatne", "ostatné") \
  X(S_OLD_PLANES, "stara data", "stará data", "stare data", "staré dáta") \
  X(S_NO_PLANE_DATA, "bez dat o letadlech", "bez dat o letadlech", "bez dat o lietadlach", "bez dát o lietadlách") \
  X(S_PAUSE, "pauza", "pauza", "pauza", "pauza") \
  X(S_IN_WINDOW, "v okne", "v okně", "v okne", "v okne") \
  X(S_NEARBY, "nablizku", "nablízku", "nablizku", "nablízku")

enum StrId : uint16_t {
#define X(id, disp, web, skDisp, skWeb) id,
  STRINGS(X)
#undef X
  STR_COUNT
};

// Pro DISPLEJ: s vyhlazenym pismem s diakritikou, s pixelovym jen ASCII.
const char* T(StrId id);

// Pro prohlizec / web - skutecne UTF-8 s diakritikou.
const char* TW(StrId id);

// Je zvolena slovencina?
bool Lang_IsSk();
// Kod jazyka pro webove sluzby ("cs" / "sk").
const char* Lang_Code();

// Text psany primo v kodu: ceska a slovenska verze, kazda bez diakritiky (pro
// pixelove pismo) a s ni (pro vyhlazene).
const char* Lang_Pick(const char* csPlain, const char* csAcc,
                      const char* skPlain, const char* skAcc);
#define LT(csPlain, csAcc, skPlain, skAcc) Lang_Pick(csPlain, csAcc, skPlain, skAcc)

// Kalendarni nazvy pro displej (s diakritikou jen s vyhlazenym pismem). wday 0 = nedele (odpovida struct tm), mon 0 = leden.
const char* Lang_WeekdayShort(int wday);
const char* Lang_MonthName(int mon);
