// =============================================================================
//  ESPD35_MeteoPlaneRadar - ulozeni nastaveni do NVS.
//  Poloha, jas, jednotky = zapis okamzity (meni se zridka).
//  Stav UI (rozsahy, obrazovka, orientace) = zapis odlozeny, viz Settings_Tick.
// =============================================================================
#include "Settings.h"
#include <Preferences.h>

static Preferences prefs;
static const char* NS = "meteoplane";

static double  s_lat = DEFAULT_LAT;
static double  s_lon = DEFAULT_LON;
static bool    s_hasLoc = false;
static uint8_t s_bl = 80;
static bool    s_metric = false;   // vychozi letecke jednotky

// --- Stav UI s odlozenym zapisem ---
static uint8_t  s_rngP = 1;        // rozsah letadel  (vychozi 25 km)
static uint8_t  s_rngM = 1;        // rozsah meteo    (vychozi 50 km)
static uint8_t  s_scr  = 0;        // aktivni obrazovka (0 = letadla)
static uint16_t s_top  = 0;        // azimut nahore (0 = sever nahoru)
static bool          s_uiDirty   = false;
static unsigned long s_uiDirtyAt = 0;

// Doba klidu, po ktere se cekajici zmeny zapisou.
static const unsigned long UI_FLUSH_DELAY_MS = 2000;

void Settings_Begin() {
  if (prefs.begin(NS, true)) {
    s_lat    = prefs.getDouble("lat", DEFAULT_LAT);
    s_lon    = prefs.getDouble("lon", DEFAULT_LON);
    s_hasLoc = prefs.getBool("hasLoc", false);
    s_bl     = prefs.getUChar("bl", 80);
    s_metric = prefs.getBool("metric", false);
    s_rngP   = prefs.getUChar("rngP", 1);
    s_rngM   = prefs.getUChar("rngM", 1);
    s_scr    = prefs.getUChar("scr", 0);
    s_top    = prefs.getUShort("topb", 0);
    prefs.end();
  }
}

double Settings_Lat() { return s_lat; }
double Settings_Lon() { return s_lon; }
bool   Settings_HasLocation() { return s_hasLoc; }

void Settings_SetLocation(double lat, double lon) {
  // Ochrana proti nesmyslu z portalu / geolokace - radeji nechat starou polohu.
  if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) return;
  if (lat == 0.0 && lon == 0.0) return;
  s_lat = lat; s_lon = lon; s_hasLoc = true;
  if (prefs.begin(NS, false)) {
    prefs.putDouble("lat", lat);
    prefs.putDouble("lon", lon);
    prefs.putBool("hasLoc", true);
    prefs.end();
  }
}

uint8_t Settings_Backlight() { return s_bl; }

void Settings_SetBacklight(uint8_t pct) {
  if (pct < 10) pct = 10;
  if (pct > 100) pct = 100;
  if (pct == s_bl) return;
  s_bl = pct;
  if (prefs.begin(NS, false)) { prefs.putUChar("bl", pct); prefs.end(); }
}

bool Settings_MetricUnits() { return s_metric; }

void Settings_SetMetricUnits(bool metric) {
  if (metric == s_metric) return;
  s_metric = metric;
  if (prefs.begin(NS, false)) { prefs.putBool("metric", metric); prefs.end(); }
}

uint8_t Settings_PlaneRange() { return s_rngP; }
void    Settings_SetPlaneRange(uint8_t idx) {
  if (idx != s_rngP) { s_rngP = idx; s_uiDirty = true; s_uiDirtyAt = millis(); }
}

uint8_t Settings_MeteoRange() { return s_rngM; }
void    Settings_SetMeteoRange(uint8_t idx) {
  if (idx != s_rngM) { s_rngM = idx; s_uiDirty = true; s_uiDirtyAt = millis(); }
}

uint8_t Settings_Screen() { return s_scr; }
void    Settings_SetScreen(uint8_t idx) {
  if (idx != s_scr) { s_scr = idx; s_uiDirty = true; s_uiDirtyAt = millis(); }
}

uint16_t Settings_TopBearing() { return s_top; }
void     Settings_SetTopBearing(uint16_t deg) {
  deg %= 360;
  if (deg != s_top) { s_top = deg; s_uiDirty = true; s_uiDirtyAt = millis(); }
}

void Settings_Tick() {
  if (!s_uiDirty) return;
  if (millis() - s_uiDirtyAt < UI_FLUSH_DELAY_MS) return;
  if (prefs.begin(NS, false)) {
    prefs.putUChar("rngP", s_rngP);
    prefs.putUChar("rngM", s_rngM);
    prefs.putUChar("scr",  s_scr);
    prefs.putUShort("topb", s_top);
    prefs.end();
  }
  s_uiDirty = false;
}

void Settings_ClearAll() {
  if (prefs.begin(NS, false)) { prefs.clear(); prefs.end(); }
  s_lat = DEFAULT_LAT; s_lon = DEFAULT_LON; s_hasLoc = false; s_bl = 80;
  s_metric = false;
  s_rngP = 1; s_rngM = 1; s_scr = 0; s_top = 0; s_uiDirty = false;
}
