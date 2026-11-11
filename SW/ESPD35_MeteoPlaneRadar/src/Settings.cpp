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
static uint8_t  s_scrMask = SCREEN_MASK_DEFAULT;   // energie vychozi vypnute
static uint16_t s_rotSec  = 0;        // 0 = stridani vypnute

// --- Meteoradar ---
static bool     s_mtLegend = true;    // legenda dBZ / mm/h

// --- Energie ---
static uint16_t s_priceFee = 0;       // Kc/MWh navic k burzovni cene
static uint8_t  s_priceVat = 0;       // procenta, 0 = bez DPH
static char     s_mixCC[4] = MIX_COUNTRY_DEFAULT;

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
static bool          s_migrateScr = false;   // prevest stary klic "scr"/"scr2" na "scr3"

// --- WiFi a heslo spravce ---
// Az WIFI_SLOTS siti (0.8.0). Slot 0 je ta, kterou drzely puvodni klice
// "wifiSsid"/"wifiPass", takze aktualizovana deska svou sit nezapomene.
static char s_ssid[WIFI_SLOTS][33];
static char s_pass[WIFI_SLOTS][65];
static char s_admin[33] = "";
static const char* const WIFI_SSID_KEY[WIFI_SLOTS] = { "wifiSsid", "wifiSsid1", "wifiSsid2" };
static const char* const WIFI_PASS_KEY[WIFI_SLOTS] = { "wifiPass", "wifiPass1", "wifiPass2" };

// --- Funkce z 0.8.x ---
static uint8_t  s_radarSrc = RADAR_SRC_CHMU;
static uint8_t  s_clkFont  = CLOCK_FONT_DEFAULT;
static uint8_t  s_lang     = LANG_DEFAULT;
static uint16_t s_altBand[ALT_BANDS_N] = { ALT_BAND0_FT_DEFAULT, ALT_BAND1_FT_DEFAULT,
                                           ALT_BAND2_FT_DEFAULT, ALT_BAND3_FT_DEFAULT };
static uint16_t s_altCol[ALT_BANDS_N + 1] = { ALT_COL0_DEFAULT, ALT_COL1_DEFAULT, ALT_COL2_DEFAULT,
                                              ALT_COL3_DEFAULT, ALT_COL4_DEFAULT };
static const char* const ALT_BAND_KEY[ALT_BANDS_N]    = { "ab0", "ab1", "ab2", "ab3" };
static const char* const ALT_COL_KEY[ALT_BANDS_N + 1] = { "acol0", "acol1", "acol2", "acol3", "acol4" };
static bool     s_stOnly = false;
static uint8_t  s_stDbz  = STORM_DBZ_DEFAULT;
static uint16_t s_stKm   = STORM_KM_DEFAULT;
static bool     s_paOn   = false;
static uint16_t s_paKm   = PLANE_ALERT_KM_DEFAULT;
static uint16_t s_paSec  = PLANE_ALERT_SEC_DEFAULT;
static bool     s_winOn  = false;
static uint16_t s_winW   = WINDOW_WIDTH_DEFAULT;
static uint16_t s_winKm  = WINDOW_KM_DEFAULT;
static bool     s_haOn   = HA_DEFAULT_ON;

// Doba klidu, po ktere se cekajici zmeny zapisou.
static const unsigned long UI_FLUSH_DELAY_MS = 2000;

// Kratke pomucky, aby se prefs.begin/end neopakovalo u kazdeho setteru.
static void putU8(const char* k, uint8_t v)      { if (prefs.begin(NS, false)) { prefs.putUChar(k, v);   prefs.end(); } }
static void putU16(const char* k, uint16_t v)    { if (prefs.begin(NS, false)) { prefs.putUShort(k, v);  prefs.end(); } }
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

    s_scrMask = prefs.getUChar("scrMask", SCREEN_MASK_DEFAULT) & SCREEN_MASK_ALL;
    s_rotSec  = prefs.getUShort("rotSec", 0);

    s_mtLegend = prefs.getBool("mtLeg", true);
    s_priceFee = prefs.getUShort("priceFee", 0);
    s_priceVat = prefs.getUChar("priceVat", 0);
    loadStr("mixCC", s_mixCC, sizeof(s_mixCC));
    if (strlen(s_mixCC) != 2) strncpy(s_mixCC, MIX_COUNTRY_DEFAULT, sizeof(s_mixCC) - 1);

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
    //
    // S energetickymi obrazovkami se to stalo podruhe: Nastaveni bylo 4, nove
    // je 6, a na indexu 4 je ted cena elektriny (ve vychozim stavu vypnuta).
    // Proto klic "scr3" a jednorazovy prevod z "scr2".
    if (prefs.isKey("scr3")) {
      s_scr = prefs.getUChar("scr3", SCREEN_PLANES_I);
    } else if (prefs.isKey("scr2")) {
      const uint8_t old2 = prefs.getUChar("scr2", SCREEN_PLANES_I);
      s_scr = (old2 == 4) ? SCREEN_SETTINGS_I : old2;   // 0..3 se nezmenily
      s_migrateScr = true;
    } else {
      const uint8_t oldScr = prefs.getUChar("scr", 0);
      s_scr = (oldScr == 0) ? SCREEN_PLANES_I
            : (oldScr == 1) ? SCREEN_METEO_I
                            : SCREEN_SETTINGS_I;
      s_migrateScr = true;      // zapise se pri prvnim Settings_Tick
    }

    for (int i = 0; i < WIFI_SLOTS; i++) {
      s_ssid[i][0] = '\0'; s_pass[i][0] = '\0';
      // isKey() nejdriv: getString() na chybejici klic pise do logu chybu za
      // kazdy prazdny slot pri kazdem startu.
      if (prefs.isKey(WIFI_SSID_KEY[i])) loadStr(WIFI_SSID_KEY[i], s_ssid[i], sizeof(s_ssid[i]));
      if (prefs.isKey(WIFI_PASS_KEY[i])) loadStr(WIFI_PASS_KEY[i], s_pass[i], sizeof(s_pass[i]));
    }
    loadStr("admPw",    s_admin, sizeof(s_admin));

    s_radarSrc = prefs.getUChar("radSrc", RADAR_SRC_CHMU);
    s_clkFont  = prefs.getUChar("clkF", CLOCK_FONT_DEFAULT);
    s_lang     = prefs.getUChar("lang", LANG_DEFAULT);
    for (int i = 0; i < ALT_BANDS_N; i++)     s_altBand[i] = prefs.getUShort(ALT_BAND_KEY[i], s_altBand[i]);
    for (int i = 0; i < ALT_BANDS_N + 1; i++) s_altCol[i]  = prefs.getUShort(ALT_COL_KEY[i], s_altCol[i]);
    s_stOnly = prefs.getBool("stOnly", false);
    s_stDbz  = prefs.getUChar("stDbz", STORM_DBZ_DEFAULT);
    s_stKm   = prefs.getUShort("stKm", STORM_KM_DEFAULT);
    s_paOn   = prefs.getBool("paOn", false);
    s_paKm   = prefs.getUShort("paKm", PLANE_ALERT_KM_DEFAULT);
    s_paSec  = prefs.getUShort("paSec", PLANE_ALERT_SEC_DEFAULT);
    s_winOn  = prefs.getBool("winOn", false);
    s_winW   = prefs.getUShort("winW", WINDOW_WIDTH_DEFAULT);
    s_winKm  = prefs.getUShort("winKm", WINDOW_KM_DEFAULT);
    s_haOn   = prefs.getBool("haOn", HA_DEFAULT_ON);
    prefs.end();
  }
  // Pojistka proti nesmyslu v NVS (poskozeny zapis, rucni uprava).
  if (s_altLo > s_altHi) { s_altLo = ALT_MIN_FT_DEFAULT; s_altHi = ALT_MAX_FT_DEFAULT; }
  if (s_nightOff >  NIGHT_OFFSET_MIN_LIMIT) s_nightOff =  NIGHT_OFFSET_MIN_LIMIT;
  if (s_nightOff < -NIGHT_OFFSET_MIN_LIMIT) s_nightOff = -NIGHT_OFFSET_MIN_LIMIT;
  if (s_scr >= SCREEN_N) s_scr = SCREEN_PLANES_I;
  if (s_radarSrc != RADAR_SRC_CHMU_MASKED) s_radarSrc = RADAR_SRC_CHMU;
  if (s_clkFont > CLOCK_FONT_MAX) s_clkFont = CLOCK_FONT_DEFAULT;
  if (s_lang > LANG_MAX) s_lang = LANG_DEFAULT;
  {
    bool ok = s_altBand[0] > 0;
    for (int i = 1; i < ALT_BANDS_N; i++) ok = ok && s_altBand[i - 1] < s_altBand[i];
    if (!ok) {
      s_altBand[0] = ALT_BAND0_FT_DEFAULT; s_altBand[1] = ALT_BAND1_FT_DEFAULT;
      s_altBand[2] = ALT_BAND2_FT_DEFAULT; s_altBand[3] = ALT_BAND3_FT_DEFAULT;
    }
  }
  if (s_priceFee > 20000) s_priceFee = 20000;
  if (s_priceVat > 100)   s_priceVat = 100;
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

// --- Meteoradar ---------------------------------------------------------------
bool Settings_MeteoLegend() { return s_mtLegend; }
void Settings_SetMeteoLegend(bool on) {
  if (on == s_mtLegend) return;
  s_mtLegend = on;
  putBool("mtLeg", on);
}

// --- Energie ----------------------------------------------------------------
uint16_t Settings_PriceFee() { return s_priceFee; }
void     Settings_SetPriceFee(uint16_t czkPerMwh) {
  // Prirazka vetsi nez nejvyssi kdy zaznamenana spotova cena je preklep -
  // skoro vzdy Kc/kWh napsane do pole v Kc/MWh. Strop zabrani tomu, aby se cely
  // prstenec obarvil jednou barvou, a hodnotu pritom tise nezahodi.
  if (czkPerMwh > 20000) czkPerMwh = 20000;
  if (czkPerMwh == s_priceFee) return;
  s_priceFee = czkPerMwh;
  putU16("priceFee", czkPerMwh);
}

uint8_t Settings_PriceVat() { return s_priceVat; }
void    Settings_SetPriceVat(uint8_t pct) {
  if (pct > 100) pct = 100;
  if (pct == s_priceVat) return;
  s_priceVat = pct;
  putU8("priceVat", pct);
}

const char* Settings_MixCountry() { return s_mixCC; }
void Settings_SetMixCountry(const char* code) {
  // Presne dve pismena, malymi. Cokoli jineho se odmitne, ne ulozi: spatny kod
  // by kazdych patnact minut skoncil na 404 a obrazovka by vypadala rozbite
  // bez jakehokoli vysvetleni.
  if (!code) return;
  char c[4] = {0};
  int n = 0;
  for (const char* p = code; *p; p++) {
    if (n >= 2) return;
    if (*p >= 'A' && *p <= 'Z')      c[n++] = (char)(*p - 'A' + 'a');
    else if (*p >= 'a' && *p <= 'z') c[n++] = *p;
    else return;
  }
  if (n != 2 || strcmp(c, s_mixCC) == 0) return;
  strncpy(s_mixCC, c, sizeof(s_mixCC) - 1);
  s_mixCC[sizeof(s_mixCC) - 1] = '\0';
  putStr("mixCC", s_mixCC);
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
const char* Settings_WifiSsid(uint8_t slot) { return slot < WIFI_SLOTS ? s_ssid[slot] : ""; }
const char* Settings_WifiPass(uint8_t slot) { return slot < WIFI_SLOTS ? s_pass[slot] : ""; }
bool Settings_HasWifi() {
  for (int i = 0; i < WIFI_SLOTS; i++) if (s_ssid[i][0]) return true;
  return false;
}

static void saveWifiSlots() {
  if (!prefs.begin(NS, false)) return;
  for (int i = 0; i < WIFI_SLOTS; i++) {
    if (s_ssid[i][0]) {
      prefs.putString(WIFI_SSID_KEY[i], s_ssid[i]);
      prefs.putString(WIFI_PASS_KEY[i], s_pass[i]);
    } else {
      if (prefs.isKey(WIFI_SSID_KEY[i])) prefs.remove(WIFI_SSID_KEY[i]);
      if (prefs.isKey(WIFI_PASS_KEY[i])) prefs.remove(WIFI_PASS_KEY[i]);
    }
  }
  prefs.end();
}

// Z portalu: zapamatovat si tuto sit. Kdyz uz ji deska zna, zmeni se jen
// heslo; jinak jde na prvni misto a ostatni se posunou (posledni vypadne).
void Settings_SetWifi(const char* ssid, const char* pass) {
  if (!ssid || !ssid[0]) return;
  int at = -1;
  for (int i = 0; i < WIFI_SLOTS; i++) if (!strcmp(s_ssid[i], ssid)) { at = i; break; }
  if (at < 0) {
    for (int i = WIFI_SLOTS - 1; i > 0; i--) {
      memcpy(s_ssid[i], s_ssid[i - 1], sizeof(s_ssid[i]));
      memcpy(s_pass[i], s_pass[i - 1], sizeof(s_pass[i]));
    }
    at = 0;
    strncpy(s_ssid[0], ssid, sizeof(s_ssid[0]) - 1);
    s_ssid[0][sizeof(s_ssid[0]) - 1] = '\0';
  }
  strncpy(s_pass[at], pass ? pass : "", sizeof(s_pass[at]) - 1);
  s_pass[at][sizeof(s_pass[at]) - 1] = '\0';
  saveWifiSlots();
}

// Z webu: zapsat jeden slot presne. Prazdne SSID slot vymaze; pass == nullptr
// necha ulozene heslo (stranka ho nikdy nevidi).
void Settings_SetWifiSlot(uint8_t slot, const char* ssid, const char* pass) {
  if (slot >= WIFI_SLOTS || !ssid) return;
  if (!ssid[0]) {
    s_ssid[slot][0] = '\0';
    s_pass[slot][0] = '\0';
  } else {
    const bool same = !strcmp(s_ssid[slot], ssid);
    strncpy(s_ssid[slot], ssid, sizeof(s_ssid[slot]) - 1);
    s_ssid[slot][sizeof(s_ssid[slot]) - 1] = '\0';
    if (pass) {
      strncpy(s_pass[slot], pass, sizeof(s_pass[slot]) - 1);
      s_pass[slot][sizeof(s_pass[slot]) - 1] = '\0';
    } else if (!same) {
      s_pass[slot][0] = '\0';     // nova sit nemuze zdedit cizi heslo
    }
  }
  saveWifiSlots();
}

void Settings_ClearWifi() {
  for (int i = 0; i < WIFI_SLOTS; i++) { s_ssid[i][0] = '\0'; s_pass[i][0] = '\0'; }
  saveWifiSlots();
}

// --- Zdroj meteoradaru -------------------------------------------------------
uint8_t Settings_RadarSource() { return s_radarSrc; }
void    Settings_SetRadarSource(uint8_t src) {
  src = (src == RADAR_SRC_CHMU_MASKED) ? RADAR_SRC_CHMU_MASKED : RADAR_SRC_CHMU;
  if (src == s_radarSrc) return;
  s_radarSrc = src;
  putU8("radSrc", src);
}

// --- Pismo ------------------------------------------------------------------
uint8_t Settings_ClockFont() { return s_clkFont; }
void    Settings_SetClockFont(uint8_t f) {
  if (f > CLOCK_FONT_MAX) f = CLOCK_FONT_DEFAULT;
  if (f == s_clkFont) return;
  s_clkFont = f;
  putU8("clkF", f);
}

// --- Jazyk ------------------------------------------------------------------
uint8_t Settings_Lang() { return s_lang; }
void    Settings_SetLang(uint8_t l) {
  if (l > LANG_MAX) l = LANG_DEFAULT;
  if (l == s_lang) return;
  s_lang = l;
  putU8("lang", l);
}

// --- Barvy vysek ------------------------------------------------------------
uint16_t Settings_AltBandFt(uint8_t i) { return i < ALT_BANDS_N ? s_altBand[i] : 0xFFFF; }
bool Settings_SetAltBandsFt(const uint16_t* b) {
  // Mimo poradi se odmita CELE, ne napul: pasma by se prekryvala a letadlo
  // by se veslo do dvou barev najednou.
  if (!b || b[0] == 0) return false;
  for (int i = 1; i < ALT_BANDS_N; i++) if (b[i - 1] >= b[i]) return false;
  if (b[ALT_BANDS_N - 1] > 60000) return false;
  for (int i = 0; i < ALT_BANDS_N; i++) s_altBand[i] = b[i];
  if (prefs.begin(NS, false)) {
    for (int i = 0; i < ALT_BANDS_N; i++) prefs.putUShort(ALT_BAND_KEY[i], s_altBand[i]);
    prefs.end();
  }
  return true;
}
uint16_t Settings_AltColor(uint8_t band) { return band <= ALT_BANDS_N ? s_altCol[band] : 0xFFFF; }
void     Settings_SetAltColor(uint8_t band, uint16_t c) {
  if (band > ALT_BANDS_N || s_altCol[band] == c) return;
  s_altCol[band] = c;
  putU16(ALT_COL_KEY[band], c);
}

// --- Hlidani bourky ---------------------------------------------------------
bool     Settings_StormOnly() { return s_stOnly; }
void     Settings_SetStormOnly(bool on) { if (on != s_stOnly) { s_stOnly = on; putBool("stOnly", on); } }
uint8_t  Settings_StormDbz() { return s_stDbz; }
void     Settings_SetStormDbz(uint8_t dbz) {
  if (dbz < 4) dbz = 4;
  if (dbz > 60) dbz = 60;
  if (dbz != s_stDbz) { s_stDbz = dbz; putU8("stDbz", dbz); }
}
uint16_t Settings_StormKm() { return s_stKm; }
void     Settings_SetStormKm(uint16_t km) {
  if (km < 5) km = 5;
  if (km > 250) km = 250;          // CHMU pokryva zhruba 500 x 280 km
  if (km != s_stKm) { s_stKm = km; putU16("stKm", km); }
}

// --- Letadlo poblic, vyhled z okna -----------------------------------------
bool     Settings_PlaneAlert() { return s_paOn; }
void     Settings_SetPlaneAlert(bool on) { if (on != s_paOn) { s_paOn = on; putBool("paOn", on); } }
uint16_t Settings_PlaneAlertKm() { return s_paKm; }
void     Settings_SetPlaneAlertKm(uint16_t km) {
  if (km < 1) km = 1;
  if (km > 50) km = 50;
  if (km != s_paKm) { s_paKm = km; putU16("paKm", km); }
}
uint16_t Settings_PlaneAlertSec() { return s_paSec; }
void     Settings_SetPlaneAlertSec(uint16_t sec) {
  if (sec < 5) sec = 5;
  if (sec > 300) sec = 300;
  if (sec != s_paSec) { s_paSec = sec; putU16("paSec", sec); }
}
bool     Settings_WindowOn() { return s_winOn; }
void     Settings_SetWindowOn(bool on) { if (on != s_winOn) { s_winOn = on; putBool("winOn", on); } }
// Okno miri tam, kam je otoceny radar letadel ("Nahore" na displeji i na webu).
// Dve samostatna nastaveni rikala totez a mohla si odporovat.
uint16_t Settings_WindowAz() { return Settings_TopBearing(); }
uint16_t Settings_WindowWidth() { return s_winW; }
void     Settings_SetWindowWidth(uint16_t deg) {
  if (deg < 10) deg = 10;
  if (deg > 300) deg = 300;
  if (deg != s_winW) { s_winW = deg; putU16("winW", deg); }
}
uint16_t Settings_WindowKm() { return s_winKm; }
void     Settings_SetWindowKm(uint16_t km) {
  if (km < 1) km = 1;
  if (km > 100) km = 100;
  if (km != s_winKm) { s_winKm = km; putU16("winKm", km); }
}

// --- Home Assistant ---------------------------------------------------------
bool Settings_HaEnabled() { return s_haOn; }
void Settings_SetHaEnabled(bool on) { if (on != s_haOn) { s_haOn = on; putBool("haOn", on); } }

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

  out["meteoLegend"] = s_mtLegend;
  out["priceFee"]    = s_priceFee;
  out["priceVat"]    = s_priceVat;
  out["mixCountry"]  = s_mixCC;

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
  out["wifiSsid"] = s_ssid[0];

  out["radarSrc"]  = s_radarSrc;
  out["clockFont"] = s_clkFont;
  out["lang"]      = s_lang;
  {
    JsonArray b = out["altBandsFt"].to<JsonArray>();
    for (int i = 0; i < ALT_BANDS_N; i++) b.add(s_altBand[i]);
    JsonArray c = out["altColors"].to<JsonArray>();
    for (int i = 0; i < ALT_BANDS_N + 1; i++) c.add(s_altCol[i]);
  }
  out["stormOnly"]     = s_stOnly;
  out["stormDbz"]      = s_stDbz;
  out["stormKm"]       = s_stKm;
  out["planeAlert"]    = s_paOn;
  out["planeAlertKm"]  = s_paKm;
  out["planeAlertSec"] = s_paSec;
  out["windowOn"]      = s_winOn;
  out["windowWidth"]   = s_winW;
  out["windowKm"]      = s_winKm;
  out["haEnabled"]     = s_haOn;
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
    uint8_t m = (uint8_t)((unsigned)in["screenMask"] & SCREEN_MASK_ALL);
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
  if (in["meteoLegend"].is<bool>()) {
    bool v = in["meteoLegend"];
    if (v != s_mtLegend) { Settings_SetMeteoLegend(v); changed = true; }
  }
  if (in["priceFee"].is<unsigned>()) {
    unsigned v = in["priceFee"];
    uint16_t before = s_priceFee;
    Settings_SetPriceFee((uint16_t)(v > 20000 ? 20000 : v));
    if (before != s_priceFee) changed = true;
  }
  if (in["priceVat"].is<unsigned>()) {
    unsigned v = in["priceVat"];
    uint8_t before = s_priceVat;
    Settings_SetPriceVat((uint8_t)(v > 100 ? 100 : v));
    if (before != s_priceVat) changed = true;
  }
  if (in["mixCountry"].is<const char*>()) {
    char before[4]; strncpy(before, s_mixCC, sizeof(before));
    Settings_SetMixCountry(in["mixCountry"].as<const char*>());
    if (strcmp(before, s_mixCC) != 0) changed = true;
  }

  // --- 0.8.x ---
  if (in["radarSrc"].is<unsigned>()) {
    const uint8_t before = s_radarSrc;
    Settings_SetRadarSource((uint8_t)in["radarSrc"].as<unsigned>());
    if (before != s_radarSrc) changed = true;
  }
  if (in["lang"].is<unsigned>()) {
    const uint8_t before = s_lang;
    Settings_SetLang((uint8_t)in["lang"].as<unsigned>());
    if (before != s_lang) changed = true;
  }
  if (in["clockFont"].is<unsigned>()) {
    const uint8_t before = s_clkFont;
    Settings_SetClockFont((uint8_t)in["clockFont"].as<unsigned>());
    if (before != s_clkFont) changed = true;
  }
  {
    // Hranice jdou dovnitr jako celek, stejne jako rozsah filtru: po jedne by
    // prosly stavem mimo poradi a byly by odmitnute.
    JsonArrayConst b = in["altBandsFt"];
    if (!b.isNull() && b.size() == ALT_BANDS_N) {
      uint16_t v[ALT_BANDS_N];
      for (int i = 0; i < ALT_BANDS_N; i++) v[i] = b[i].as<uint16_t>();
      if (Settings_SetAltBandsFt(v)) changed = true;
    }
    JsonArrayConst c = in["altColors"];
    if (!c.isNull()) {
      for (size_t i = 0; i <= ALT_BANDS_N && i < c.size(); i++) {
        const uint16_t before = s_altCol[i];
        Settings_SetAltColor((uint8_t)i, c[i].as<uint16_t>());
        if (before != s_altCol[i]) changed = true;
      }
    }
  }
#define SET_BOOL(key, getter, setter) \
  if (in[key].is<bool>()) { const bool v = in[key]; if (v != getter()) { setter(v); changed = true; } }
#define SET_NUM(key, T, getter, setter) \
  if (in[key].is<unsigned>()) { const T before = getter(); setter((T)in[key].as<unsigned>()); if (before != getter()) changed = true; }
  SET_BOOL("stormOnly",  Settings_StormOnly,  Settings_SetStormOnly)
  SET_NUM ("stormDbz",   uint8_t,  Settings_StormDbz,  Settings_SetStormDbz)
  SET_NUM ("stormKm",    uint16_t, Settings_StormKm,   Settings_SetStormKm)
  SET_BOOL("planeAlert", Settings_PlaneAlert, Settings_SetPlaneAlert)
  SET_NUM ("planeAlertKm",  uint16_t, Settings_PlaneAlertKm,  Settings_SetPlaneAlertKm)
  SET_NUM ("planeAlertSec", uint16_t, Settings_PlaneAlertSec, Settings_SetPlaneAlertSec)
  SET_BOOL("windowOn",   Settings_WindowOn,   Settings_SetWindowOn)
  SET_NUM ("windowWidth", uint16_t, Settings_WindowWidth, Settings_SetWindowWidth)
  SET_NUM ("windowKm",    uint16_t, Settings_WindowKm,    Settings_SetWindowKm)
  SET_BOOL("haEnabled",  Settings_HaEnabled,  Settings_SetHaEnabled)
#undef SET_BOOL
#undef SET_NUM

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

  // Heslo spravce se tudy NEMENI. Klic "password" v tele je heslo, ktere
  // akci povoluje (obnova ze zalohy), ne nove heslo - driv se obe veci
  // pletly a preklep v hesle pro reset se pri Ulozit ulozil jako nove heslo.
  // Zmena hesla ma vlastni koncovy bod /api/password (WebConfig.cpp).

  return changed;
}

// --- Odlozeny zapis ---------------------------------------------------------
void Settings_Tick() {
  if (!s_uiDirty) return;
  if (millis() - s_uiDirtyAt < UI_FLUSH_DELAY_MS) return;
  if (prefs.begin(NS, false)) {
    prefs.putUChar("rngP", s_rngP);
    prefs.putUChar("rngM", s_rngM);
    prefs.putUChar("scr3", s_scr);
    prefs.putUShort("topb", s_top);
    if (s_migrateScr) {
      // Stary klic uz nikdo necte; smazat ho znamena, ze se prevod neprovede
      // podruhe, kdyby se nekdy vratil starsi firmware a zapsal do nej.
      prefs.remove("scr");
      prefs.remove("scr2");
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
  s_scrMask = SCREEN_MASK_DEFAULT; s_rotSec = 0;
  s_mtLegend = true; s_priceFee = 0; s_priceVat = 0;
  strncpy(s_mixCC, MIX_COUNTRY_DEFAULT, sizeof(s_mixCC) - 1); s_mixCC[sizeof(s_mixCC) - 1] = '\0';
  s_secStyle = SEC_STYLE_COMET; s_clkColor = 0xFFFF; s_secColor = 0x05FF;
  s_rngP = 1; s_rngM = 1; s_scr = SCREEN_PLANES_I; s_top = 0;
  s_uiDirty = false; s_migrateScr = false;
  for (int i = 0; i < WIFI_SLOTS; i++) { s_ssid[i][0] = '\0'; s_pass[i][0] = '\0'; }
  s_admin[0] = '\0';
  s_radarSrc = RADAR_SRC_CHMU; s_clkFont = CLOCK_FONT_DEFAULT; s_lang = LANG_DEFAULT;
  s_altBand[0] = ALT_BAND0_FT_DEFAULT; s_altBand[1] = ALT_BAND1_FT_DEFAULT;
  s_altBand[2] = ALT_BAND2_FT_DEFAULT; s_altBand[3] = ALT_BAND3_FT_DEFAULT;
  s_altCol[0] = ALT_COL0_DEFAULT; s_altCol[1] = ALT_COL1_DEFAULT; s_altCol[2] = ALT_COL2_DEFAULT;
  s_altCol[3] = ALT_COL3_DEFAULT; s_altCol[4] = ALT_COL4_DEFAULT;
  s_stOnly = false; s_stDbz = STORM_DBZ_DEFAULT; s_stKm = STORM_KM_DEFAULT;
  s_paOn = false; s_paKm = PLANE_ALERT_KM_DEFAULT; s_paSec = PLANE_ALERT_SEC_DEFAULT;
  s_winOn = false; s_winW = WINDOW_WIDTH_DEFAULT; s_winKm = WINDOW_KM_DEFAULT;
  s_haOn = HA_DEFAULT_ON;
}
