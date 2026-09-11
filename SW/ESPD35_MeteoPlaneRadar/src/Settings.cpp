// =============================================================================
//  ESPD35_MeteoPlaneRadar - ulozeni nastaveni do NVS.
//
//  Poloha, jas, jednotky, filtry = zapis okamzity (meni se zridka).
//  Stav UI (rozsahy, obrazovka, orientace) = zapis odlozeny, viz Settings_Tick.
//
//  Nazvy klicu jsou zamerne kratke - NVS ma limit 15 znaku - a od verze 0.3.0
//  se NEMENI, aby deska po aktualizaci nasla sve nastaveni tam, kde ho nechala.
// =============================================================================
#include "Settings.h"
#include <Preferences.h>
#include <string.h>
#include <ctype.h>

static Preferences prefs;
static const char* NS = "meteoplane";

static double  s_lat = DEFAULT_LAT;
static double  s_lon = DEFAULT_LON;
static bool    s_hasLoc = false;
static bool    s_metric = false;   // vychozi letecke jednotky

// --- Jas: dve urovne + ktera prave plati ---
static uint8_t s_blDay     = BRIGHT_DAY_DEFAULT;
static uint8_t s_blNight   = BRIGHT_NIGHT_DEFAULT;
static bool    s_nightAuto = false;
static int8_t  s_nightOff  = 0;
static bool    s_isNight   = false;   // behovy stav, do NVS se neuklada

// --- Filtry letadel ---
static uint16_t s_altLo   = ALT_MIN_FT_DEFAULT;
static uint16_t s_altHi   = ALT_MAX_FT_DEFAULT;
static bool     s_onlyCs  = false;
static bool     s_sqAlert = true;     // nouzovy squawk stoji za zvyrazneni
static char     s_watch[10] = "";     // hlidany volaci znak, "" = zadny

// --- Obrazovky a stridani ---
// Bitova maska zapnutych datovych obrazovek. Nastaveni (SCREEN_SETTINGS_I)
// v masce neni - je vzdy zapnute.
static uint8_t  s_scrMask = 0x0F;     // vychozi: vsechny ctyri zapnute
static uint16_t s_rotSec  = 0;        // 0 = stridani vypnute

// --- Vzhled hodin ---
static uint8_t  s_secStyle = SEC_STYLE_COMET;
static uint16_t s_clkColor = 0xFFFF;  // bila
static uint16_t s_secColor = 0x05FF;  // azurova

// --- Stav UI s odlozenym zapisem ---
static uint8_t  s_rngP = 1;        // rozsah letadel  (vychozi 25 km)
static uint8_t  s_rngM = 1;        // rozsah meteo    (vychozi 50 km)
static uint8_t  s_scr  = 0;        // aktivni obrazovka (0 = letadla)
static uint16_t s_top  = 0;        // azimut nahore (0 = sever nahoru)
static bool          s_uiDirty   = false;
static unsigned long s_uiDirtyAt = 0;
static bool          s_migrateScr = false;   // prevest stary klic "scr" na "scr2"

// --- WiFi a heslo spravce ---
static char s_ssid[33]  = "";
static char s_pass[65]  = "";
static char s_admin[33] = "";

// Doba klidu, po ktere se cekajici zmeny zapisou.
static const unsigned long UI_FLUSH_DELAY_MS = 2000;

// Kratke pomucky, aby se prefs.begin/end neopakovalo u kazdeho setteru.
static void putU8(const char* k, uint8_t v)      { if (prefs.begin(NS, false)) { prefs.putUChar(k, v);   prefs.end(); } }
static void putBool(const char* k, bool v)       { if (prefs.begin(NS, false)) { prefs.putBool(k, v);    prefs.end(); } }
static void putStr(const char* k, const char* v) { if (prefs.begin(NS, false)) { prefs.putString(k, v);  prefs.end(); } }

static void loadStr(const char* k, char* dst, size_t cap) {
  String v = prefs.getString(k, "");
  strncpy(dst, v.c_str(), cap - 1);
  dst[cap - 1] = '\0';
}

void Settings_Begin() {
  if (prefs.begin(NS, true)) {
    s_lat    = prefs.getDouble("lat", DEFAULT_LAT);
    s_lon    = prefs.getDouble("lon", DEFAULT_LON);
    s_hasLoc = prefs.getBool("hasLoc", false);
    s_metric = prefs.getBool("metric", false);

    // "bl" je od 0.4.0 DENNI uroven jasu. Deska aktualizovana z 0.3.0 tak
    // dostane svuj dosavadni jas jako denni a nemusi ho nastavovat znovu.
    s_blDay     = prefs.getUChar("bl", BRIGHT_DAY_DEFAULT);
    s_blNight   = prefs.getUChar("blNight", BRIGHT_NIGHT_DEFAULT);
    s_nightAuto = prefs.getBool("nightAuto", false);
    s_nightOff  = (int8_t)prefs.getChar("nightOff", 0);

    s_altLo   = prefs.getUShort("altLo", ALT_MIN_FT_DEFAULT);
    s_altHi   = prefs.getUShort("altHi", ALT_MAX_FT_DEFAULT);
    s_onlyCs  = prefs.getBool("onlyCs", false);
    s_sqAlert = prefs.getBool("sqAlert", true);
    loadStr("watchCs", s_watch, sizeof(s_watch));

    s_scrMask = prefs.getUChar("scrMask", 0x0F);
    s_rotSec  = prefs.getUShort("rotSec", 0);

    s_secStyle = prefs.getUChar("secStyle", SEC_STYLE_COMET);
    s_clkColor = prefs.getUShort("clkCol", 0xFFFF);
    s_secColor = prefs.getUShort("secCol", 0x05FF);

    s_rngP = prefs.getUChar("rngP", 1);
    s_rngM = prefs.getUChar("rngM", 1);
    s_top  = prefs.getUShort("topb", 0);

    // Cislovani obrazovek se v 0.6.0 zmenilo (pribyly hodiny a predpoved),
    // takze stara hodnota pod klicem "scr" uz znamena neco jineho. Cte se
    // proto NOVY klic "scr2" a stary se jednorazove prevede - jinak by se
    // deska po aktualizaci probudila na jine obrazovce, nez kde ji uzivatel
    // nechal, a nikdo by nevedel proc.
    if (prefs.isKey("scr2")) {
      s_scr = prefs.getUChar("scr2", SCREEN_PLANES_I);
    } else {
      const uint8_t oldScr = prefs.getUChar("scr", 0);
      s_scr = (oldScr == 0) ? SCREEN_PLANES_I
            : (oldScr == 1) ? SCREEN_METEO_I
                            : SCREEN_SETTINGS_I;
      s_migrateScr = true;      // zapise se pri prvnim Settings_Tick
    }

    loadStr("wifiSsid", s_ssid,  sizeof(s_ssid));
    loadStr("wifiPass", s_pass,  sizeof(s_pass));
    loadStr("admPw",    s_admin, sizeof(s_admin));
    prefs.end();
  }
  // Pojistka proti nesmyslu v NVS (poskozeny zapis, rucni uprava).
  if (s_altLo > s_altHi) { s_altLo = ALT_MIN_FT_DEFAULT; s_altHi = ALT_MAX_FT_DEFAULT; }
  if (s_nightOff >  NIGHT_OFFSET_MIN_LIMIT) s_nightOff =  NIGHT_OFFSET_MIN_LIMIT;
  if (s_nightOff < -NIGHT_OFFSET_MIN_LIMIT) s_nightOff = -NIGHT_OFFSET_MIN_LIMIT;
}

// --- Poloha -----------------------------------------------------------------
double Settings_Lat() { return s_lat; }
double Settings_Lon() { return s_lon; }
bool   Settings_HasLocation() { return s_hasLoc; }

void Settings_SetLocation(double lat, double lon) {
  // Ochrana proti nesmyslu z portalu / geolokace - radeji nechat starou polohu.
  // NaN projde i obracenym porovnanim, proto se testuje platny rozsah, ne jeho
  // negace.
  if (!(lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0)) return;
  if (lat == 0.0 && lon == 0.0) return;
  s_lat = lat; s_lon = lon; s_hasLoc = true;
  if (prefs.begin(NS, false)) {
    prefs.putDouble("lat", lat);
    prefs.putDouble("lon", lon);
    prefs.putBool("hasLoc", true);
    prefs.end();
  }
}

// --- Jas --------------------------------------------------------------------
static uint8_t clampPct(uint8_t pct) {
  if (pct < 10) return 10;      // uplna tma vypada jako rozbita deska
  if (pct > 100) return 100;
  return pct;
}

uint8_t Settings_Backlight() { return s_isNight ? s_blNight : s_blDay; }

void Settings_SetBacklight(uint8_t pct) {
  if (s_isNight) Settings_SetBrightNight(pct);
  else           Settings_SetBrightDay(pct);
}

uint8_t Settings_BrightDay()   { return s_blDay; }
uint8_t Settings_BrightNight() { return s_blNight; }

void Settings_SetBrightDay(uint8_t pct) {
  pct = clampPct(pct);
  if (pct == s_blDay) return;
  s_blDay = pct;
  putU8("bl", pct);
}

void Settings_SetBrightNight(uint8_t pct) {
  pct = clampPct(pct);
  if (pct == s_blNight) return;
  s_blNight = pct;
  putU8("blNight", pct);
}

bool Settings_NightAuto() { return s_nightAuto; }
void Settings_SetNightAuto(bool on) {
  if (on == s_nightAuto) return;
  s_nightAuto = on;
  putBool("nightAuto", on);
}

int8_t Settings_NightOffsetMin() { return s_nightOff; }
void   Settings_SetNightOffsetMin(int8_t m) {
  if (m >  NIGHT_OFFSET_MIN_LIMIT) m =  NIGHT_OFFSET_MIN_LIMIT;
  if (m < -NIGHT_OFFSET_MIN_LIMIT) m = -NIGHT_OFFSET_MIN_LIMIT;
  if (m == s_nightOff) return;
  s_nightOff = m;
  if (prefs.begin(NS, false)) { prefs.putChar("nightOff", m); prefs.end(); }
}

bool Settings_IsNight() { return s_isNight; }
void Settings_SetNight(bool night) { s_isNight = night; }

// --- Jednotky ---------------------------------------------------------------
bool Settings_MetricUnits() { return s_metric; }

void Settings_SetMetricUnits(bool metric) {
  if (metric == s_metric) return;
  s_metric = metric;
  putBool("metric", metric);
}

// --- Filtry -----------------------------------------------------------------
uint16_t Settings_AltMinFt() { return s_altLo; }
uint16_t Settings_AltMaxFt() { return s_altHi; }

void Settings_SetAltRangeFt(uint16_t lo, uint16_t hi) {
  if (lo > hi) { uint16_t t = lo; lo = hi; hi = t; }   // prohozene meze srovnat
  if (lo == s_altLo && hi == s_altHi) return;
  s_altLo = lo; s_altHi = hi;
  if (prefs.begin(NS, false)) {
    prefs.putUShort("altLo", lo);
    prefs.putUShort("altHi", hi);
    prefs.end();
  }
}

bool Settings_OnlyWithCallsign() { return s_onlyCs; }
void Settings_SetOnlyWithCallsign(bool on) {
  if (on == s_onlyCs) return;
  s_onlyCs = on;
  putBool("onlyCs", on);
}

bool Settings_SquawkAlert() { return s_sqAlert; }
void Settings_SetSquawkAlert(bool on) {
  if (on == s_sqAlert) return;
  s_sqAlert = on;
  putBool("sqAlert", on);
}

const char* Settings_WatchCallsign() { return s_watch; }
void Settings_SetWatchCallsign(const char* v) {
  char buf[sizeof(s_watch)] = "";
  // Ulozi se velkymi pismeny a bez mezer, protoze presne tak chodi volaci znak
  // z adsb.fi - porovnavat jinak zapsany retezec by nikdy nesedelo.
  int j = 0;
  for (int i = 0; v && v[i] && j < (int)sizeof(buf) - 1; i++) {
    if (v[i] == ' ') continue;
    buf[j++] = (char)toupper((unsigned char)v[i]);
  }
  buf[j] = '\0';
  if (strcmp(buf, s_watch) == 0) return;
  strncpy(s_watch, buf, sizeof(s_watch));
  putStr("watchCs", s_watch);
}

// --- Obrazovky --------------------------------------------------------------
bool Settings_ScreenEnabled(uint8_t idx) {
  if (idx == SCREEN_SETTINGS_I) return true;    // nikdy nejde vypnout
  if (idx >= SCREEN_N) return false;
  return (s_scrMask >> idx) & 1;
}

void Settings_SetScreenEnabled(uint8_t idx, bool on) {
  if (idx >= SCREEN_SETTINGS_I) return;         // Nastaveni a nesmysly ignorovat
  uint8_t m = s_scrMask;
  if (on) m |= (uint8_t)(1u << idx);
  else    m &= (uint8_t)~(1u << idx);
  if (m == s_scrMask) return;
  s_scrMask = m;
  putU8("scrMask", m);
}

uint8_t Settings_EnabledCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < SCREEN_SETTINGS_I; i++) if (Settings_ScreenEnabled(i)) n++;
  return n;
}

uint16_t Settings_AutoRotateSec() { return s_rotSec; }
void     Settings_SetAutoRotateSec(uint16_t v) {
  // Pod deset sekund je to nepouzitelne (obrazovka se nestihne ani nacist)
  // a nad hodinu uz to neni stridani.
  if (v != 0 && v < 10) v = 10;
  if (v > 3600) v = 3600;
  if (v == s_rotSec) return;
  s_rotSec = v;
  if (prefs.begin(NS, false)) { prefs.putUShort("rotSec", v); prefs.end(); }
}

// --- Vzhled hodin -----------------------------------------------------------
uint8_t Settings_SecondsStyle() { return s_secStyle; }
void    Settings_SetSecondsStyle(uint8_t v) {
  if (v > SEC_STYLE_COMET) v = SEC_STYLE_COMET;
  if (v == s_secStyle) return;
  s_secStyle = v;
  putU8("secStyle", v);
}

uint16_t Settings_ClockColor() { return s_clkColor; }
void     Settings_SetClockColor(uint16_t c) {
  if (c == s_clkColor) return;
  s_clkColor = c;
  if (prefs.begin(NS, false)) { prefs.putUShort("clkCol", c); prefs.end(); }
}

uint16_t Settings_SecondsColor() { return s_secColor; }
void     Settings_SetSecondsColor(uint16_t c) {
  if (c == s_secColor) return;
  s_secColor = c;
  if (prefs.begin(NS, false)) { prefs.putUShort("secCol", c); prefs.end(); }
}

// --- Stav UI ----------------------------------------------------------------
static void markUiDirty() { s_uiDirty = true; s_uiDirtyAt = millis(); }

uint8_t Settings_PlaneRange() { return s_rngP; }
void    Settings_SetPlaneRange(uint8_t idx) {
  if (idx != s_rngP) { s_rngP = idx; markUiDirty(); }
}

uint8_t Settings_MeteoRange() { return s_rngM; }
void    Settings_SetMeteoRange(uint8_t idx) {
  if (idx != s_rngM) { s_rngM = idx; markUiDirty(); }
}

uint8_t Settings_Screen() { return s_scr; }
void    Settings_SetScreen(uint8_t idx) {
  if (idx != s_scr) { s_scr = idx; markUiDirty(); }
}

uint16_t Settings_TopBearing() { return s_top; }
void     Settings_SetTopBearing(uint16_t deg) {
  deg %= 360;
  if (deg != s_top) { s_top = deg; markUiDirty(); }
}

// --- WiFi -------------------------------------------------------------------
const char* Settings_WifiSsid() { return s_ssid; }
const char* Settings_WifiPass() { return s_pass; }
bool        Settings_HasWifi()  { return s_ssid[0] != '\0'; }

void Settings_SetWifi(const char* ssid, const char* pass) {
  if (!ssid) return;
  strncpy(s_ssid, ssid, sizeof(s_ssid) - 1);
  s_ssid[sizeof(s_ssid) - 1] = '\0';
  strncpy(s_pass, pass ? pass : "", sizeof(s_pass) - 1);
  s_pass[sizeof(s_pass) - 1] = '\0';
  if (prefs.begin(NS, false)) {
    prefs.putString("wifiSsid", s_ssid);
    prefs.putString("wifiPass", s_pass);
    prefs.end();
  }
}

void Settings_ClearWifi() {
  s_ssid[0] = '\0';
  s_pass[0] = '\0';
  if (prefs.begin(NS, false)) {
    prefs.remove("wifiSsid");
    prefs.remove("wifiPass");
    prefs.end();
  }
}

// --- Heslo spravce ----------------------------------------------------------
bool        Settings_HasAdminPassword() { return s_admin[0] != '\0'; }
const char* Settings_AdminPassword()    { return s_admin; }

void Settings_SetAdminPassword(const char* plain) {
  strncpy(s_admin, plain ? plain : "", sizeof(s_admin) - 1);
  s_admin[sizeof(s_admin) - 1] = '\0';
  putStr("admPw", s_admin);
}

bool Settings_CheckAdminPassword(const char* plain) {
  if (!Settings_HasAdminPassword()) return true;   // ochrana vypnuta
  if (!plain) return false;
  return strcmp(s_admin, plain) == 0;
}

// --- Serializace ------------------------------------------------------------
void Settings_ToJson(JsonObject out) {
  out["lat"]    = s_lat;
  out["lon"]    = s_lon;
  out["hasLoc"] = s_hasLoc;
  out["metric"] = s_metric;

  out["brightDay"]   = s_blDay;
  out["brightNight"] = s_blNight;
  out["nightAuto"]   = s_nightAuto;
  out["nightOffset"] = s_nightOff;
  out["isNight"]     = s_isNight;

  out["altMinFt"]         = s_altLo;
  out["altMaxFt"]         = s_altHi;
  out["onlyWithCallsign"] = s_onlyCs;
  out["squawkAlert"]      = s_sqAlert;
  out["watchCallsign"]    = s_watch;

  out["screenMask"]    = s_scrMask;
  out["autoRotateSec"] = s_rotSec;

  out["secondsStyle"] = s_secStyle;
  out["clockColor"]   = s_clkColor;
  out["secondsColor"] = s_secColor;

  out["planeRange"] = s_rngP;
  out["meteoRange"] = s_rngM;
  out["screen"]     = s_scr;
  out["topBearing"] = s_top;

  // Heslo se neposila NIKDY - stranka se dozvi jen, jestli nejake je.
  out["hasPassword"] = Settings_HasAdminPassword();
  // SSID ano (uzivatel potrebuje videt, k cemu je deska pripojena), heslo ne.
  out["wifiSsid"] = s_ssid;
}

bool Settings_FromJson(JsonObjectConst in) {
  bool changed = false;

  // Poloha se meni jen kdyz prijdou OBE souradnice - jinak by pulka zmeny
  // posunula desku nekam na polednik.
  if (in["lat"].is<double>() && in["lon"].is<double>()) {
    double lat = in["lat"], lon = in["lon"];
    if (lat != s_lat || lon != s_lon) { Settings_SetLocation(lat, lon); changed = true; }
  }
  if (in["metric"].is<bool>()) {
    bool v = in["metric"];
    if (v != s_metric) { Settings_SetMetricUnits(v); changed = true; }
  }

  if (in["brightDay"].is<unsigned>()) {
    uint8_t v = clampPct((uint8_t)(unsigned)in["brightDay"]);
    if (v != s_blDay) { Settings_SetBrightDay(v); changed = true; }
  }
  if (in["brightNight"].is<unsigned>()) {
    uint8_t v = clampPct((uint8_t)(unsigned)in["brightNight"]);
    if (v != s_blNight) { Settings_SetBrightNight(v); changed = true; }
  }
  if (in["nightAuto"].is<bool>()) {
    bool v = in["nightAuto"];
    if (v != s_nightAuto) { Settings_SetNightAuto(v); changed = true; }
  }
  if (in["nightOffset"].is<int>()) {
    int8_t v = (int8_t)(int)in["nightOffset"];
    if (v != s_nightOff) { Settings_SetNightOffsetMin(v); changed = true; }
  }

  if (in["altMinFt"].is<unsigned>() || in["altMaxFt"].is<unsigned>()) {
    uint16_t lo = in["altMinFt"] | s_altLo;
    uint16_t hi = in["altMaxFt"] | s_altHi;
    if (lo != s_altLo || hi != s_altHi) { Settings_SetAltRangeFt(lo, hi); changed = true; }
  }
  if (in["onlyWithCallsign"].is<bool>()) {
    bool v = in["onlyWithCallsign"];
    if (v != s_onlyCs) { Settings_SetOnlyWithCallsign(v); changed = true; }
  }
  if (in["squawkAlert"].is<bool>()) {
    bool v = in["squawkAlert"];
    if (v != s_sqAlert) { Settings_SetSquawkAlert(v); changed = true; }
  }
  if (in["watchCallsign"].is<const char*>()) {
    const char* v = in["watchCallsign"];
    // Porovnava se az po normalizaci (velka pismena, bez mezer), jinak by
    // "csa 123" hlasilo zmenu pokazde, i kdyz je ulozeno tote.
    char before[16]; strncpy(before, s_watch, sizeof(before) - 1); before[sizeof(before)-1] = '\0';
    Settings_SetWatchCallsign(v);
    if (strcmp(before, s_watch) != 0) changed = true;
  }

  if (in["screenMask"].is<unsigned>()) {
    uint8_t m = (uint8_t)((unsigned)in["screenMask"] & 0x0F);
    // Vsechny datove obrazovky vypnute by nechaly uzivatele jen s Nastavenim.
    // To sice neni slepa ulicka (web bezi dal), ale skoro jiste to nikdo
    // nechtel - takze se aspon jedna necha zapnuta.
    if (m == 0) m = (uint8_t)(1u << SCREEN_PLANES_I);
    if (m != s_scrMask) {
      for (uint8_t i = 0; i < SCREEN_SETTINGS_I; i++)
        Settings_SetScreenEnabled(i, (m >> i) & 1);
      changed = true;
    }
  }
  if (in["autoRotateSec"].is<unsigned>()) {
    uint16_t v = (uint16_t)(unsigned)in["autoRotateSec"];
    uint16_t before = s_rotSec;
    Settings_SetAutoRotateSec(v);
    if (before != s_rotSec) changed = true;
  }

  if (in["secondsStyle"].is<unsigned>()) {
    uint8_t v = (uint8_t)(unsigned)in["secondsStyle"];
    uint8_t before = s_secStyle;
    Settings_SetSecondsStyle(v);
    if (before != s_secStyle) changed = true;
  }
  if (in["clockColor"].is<unsigned>()) {
    uint16_t v = (uint16_t)(unsigned)in["clockColor"];
    if (v != s_clkColor) { Settings_SetClockColor(v); changed = true; }
  }
  if (in["secondsColor"].is<unsigned>()) {
    uint16_t v = (uint16_t)(unsigned)in["secondsColor"];
    if (v != s_secColor) { Settings_SetSecondsColor(v); changed = true; }
  }

  if (in["topBearing"].is<unsigned>()) {
    uint16_t v = (uint16_t)((unsigned)in["topBearing"] % 360);
    if (v != s_top) { Settings_SetTopBearing(v); changed = true; }
  }

  // Heslo je jediny klic, ktery se prijima, ale nikdy nevydava.
  if (in["password"].is<const char*>()) {
    const char* pw = in["password"];
    if (pw && strcmp(pw, s_admin) != 0) { Settings_SetAdminPassword(pw); changed = true; }
  }

  return changed;
}

// --- Odlozeny zapis ---------------------------------------------------------
void Settings_Tick() {
  if (!s_uiDirty) return;
  if (millis() - s_uiDirtyAt < UI_FLUSH_DELAY_MS) return;
  if (prefs.begin(NS, false)) {
    prefs.putUChar("rngP", s_rngP);
    prefs.putUChar("rngM", s_rngM);
    prefs.putUChar("scr2", s_scr);
    prefs.putUShort("topb", s_top);
    if (s_migrateScr) {
      // Stary klic uz nikdo necte; smazat ho znamena, ze se prevod neprovede
      // podruhe, kdyby se nekdy vratil starsi firmware a zapsal do nej.
      prefs.remove("scr");
      s_migrateScr = false;
    }
    prefs.end();
  }
  s_uiDirty = false;
}

void Settings_ClearAll() {
  if (prefs.begin(NS, false)) { prefs.clear(); prefs.end(); }
  s_lat = DEFAULT_LAT; s_lon = DEFAULT_LON; s_hasLoc = false;
  s_metric = false;
  s_blDay = BRIGHT_DAY_DEFAULT; s_blNight = BRIGHT_NIGHT_DEFAULT;
  s_nightAuto = false; s_nightOff = 0; s_isNight = false;
  s_altLo = ALT_MIN_FT_DEFAULT; s_altHi = ALT_MAX_FT_DEFAULT;
  s_onlyCs = false; s_sqAlert = true; s_watch[0] = '\0';
  s_scrMask = 0x0F; s_rotSec = 0;
  s_secStyle = SEC_STYLE_COMET; s_clkColor = 0xFFFF; s_secColor = 0x05FF;
  s_rngP = 1; s_rngM = 1; s_scr = SCREEN_PLANES_I; s_top = 0;
  s_uiDirty = false; s_migrateScr = false;
  s_ssid[0] = '\0'; s_pass[0] = '\0'; s_admin[0] = '\0';
}
