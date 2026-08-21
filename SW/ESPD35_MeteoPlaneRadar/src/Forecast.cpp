// =============================================================================
//  ESPD35_MeteoPlaneRadar
//  Open-Meteo forecast + air quality. See Forecast.h.
//
// =============================================================================
#include "Forecast.h"
#include "Net.h"
#include "Settings.h"
#include "Outside.h"
#include "Lang.h"
#include "Status.h"
#include <ArduinoJson.h>
#include <WiFi.h>

// --- Forecast state ---
static FcHour s_hours[FORECAST_HOURS];
static FcDay  s_days[FORECAST_DAYS];
static int    s_hourN = 0, s_dayN = 0;
static bool   s_valid = false;

static bool  s_curOk = false;
static float s_curTemp = 0, s_curPrecip = 0, s_curWind = 0;
static int   s_curCode = 0;

static time_t s_sunrise = 0, s_sunset = 0;

static unsigned long s_lastTry = 0;
static bool          s_everTried = false;
static bool          s_forceNext = false;

// --- Air quality state ---
static bool  s_aqOk = false;
static int   s_aqi = -1;
static float s_pm25 = 0, s_pm10 = 0;
static float s_pollen = -1;
static const char* s_pollenWorst = "";
static unsigned long s_aqLastTry = 0;
static bool          s_aqEverTried = false;

void Forecast_Invalidate() { s_forceNext = true; }

bool Forecast_Valid()        { return s_valid; }
int  Forecast_HourCount()    { return s_hourN; }
const FcHour* Forecast_Hours() { return s_hours; }
int  Forecast_DayCount()     { return s_dayN; }
const FcDay*  Forecast_Days()  { return s_days; }

bool  Forecast_CurrentValid() { return s_curOk; }
float Forecast_CurrentTemp()  { return s_curTemp; }
float Forecast_CurrentPrecip(){ return s_curPrecip; }
float Forecast_CurrentWind()  { return s_curWind; }
int   Forecast_CurrentCode()  { return s_curCode; }

bool Forecast_SunTimes(time_t* rise, time_t* set) {
  if (!s_sunrise || !s_sunset) return false;
  if (rise) *rise = s_sunrise;
  if (set)  *set  = s_sunset;
  return true;
}

// -----------------------------------------------------------------------------
//  Forecast
// -----------------------------------------------------------------------------
static bool fetchForecast() {
  char url[420];
  // forecast_hours starts at the CURRENT hour, so hourly[0] is always "now" and
  // no searching through a day-aligned array is needed.
  snprintf(url, sizeof(url),
    "%s?latitude=%.4f&longitude=%.4f"
    "&current=temperature_2m,precipitation,weather_code,wind_speed_10m"
    "&hourly=temperature_2m,precipitation,weather_code,wind_speed_10m"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_sum,"
    "wind_speed_10m_max,sunrise,sunset"
    "&timeformat=unixtime&timezone=UTC&forecast_hours=%d&forecast_days=%d",
    FORECAST_URL, Settings_Lat(), Settings_Lon(),
    FORECAST_HOURS, FORECAST_DAYS + 1);

  String body;
  if (!Net_GetString(url, body, "PREDPOVED")) {
    Status_Set(ST_FORECAST, "chyba stahovani");
    return false;
  }

  // Filter first: the raw answer carries units and metadata we never look at,
  // and on a device with 8 MB of PSRAM but very little internal RAM it is the
  // parse that hurts, not the download.
  JsonDocument filter;
  filter["current"]["temperature_2m"] = true;
  filter["current"]["precipitation"]  = true;
  filter["current"]["weather_code"]   = true;
  filter["current"]["wind_speed_10m"] = true;
  JsonObject fh = filter["hourly"].to<JsonObject>();
  fh["time"] = true; fh["temperature_2m"] = true; fh["precipitation"] = true;
  fh["weather_code"] = true; fh["wind_speed_10m"] = true;
  JsonObject fd = filter["daily"].to<JsonObject>();
  fd["time"] = true; fd["weather_code"] = true;
  fd["temperature_2m_max"] = true; fd["temperature_2m_min"] = true;
  fd["precipitation_sum"] = true; fd["wind_speed_10m_max"] = true;
  fd["sunrise"] = true; fd["sunset"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body,
                                             DeserializationOption::Filter(filter));
  body = String();                 // free the payload before touching anything else
  if (err) { Serial.printf("PREDPOVED: JSON %s\n", err.c_str()); return false; }

  // --- current ---
  JsonObjectConst cur = doc["current"];
  if (!cur.isNull() && !cur["temperature_2m"].isNull()) {
    s_curTemp   = cur["temperature_2m"].as<float>();
    s_curPrecip = cur["precipitation"] | 0.0f;
    s_curCode   = cur["weather_code"] | 0;
    s_curWind   = cur["wind_speed_10m"] | 0.0f;
    s_curOk = true;
    // The status line under the screen dots shows this too - hand it over so it
    // does not have to make the same request again.
    Outside_NoteTemp(s_curTemp);
  }

  // --- hourly ---
  JsonArrayConst ht = doc["hourly"]["time"];
  JsonArrayConst hT = doc["hourly"]["temperature_2m"];
  JsonArrayConst hP = doc["hourly"]["precipitation"];
  JsonArrayConst hC = doc["hourly"]["weather_code"];
  JsonArrayConst hW = doc["hourly"]["wind_speed_10m"];
  s_hourN = 0;
  if (!ht.isNull()) {
    for (size_t i = 0; i < ht.size() && s_hourN < FORECAST_HOURS; i++) {
      s_hours[s_hourN].t      = (time_t)ht[i].as<long long>();
      s_hours[s_hourN].temp   = hT.isNull() ? 0 : hT[i].as<float>();
      s_hours[s_hourN].precip = hP.isNull() ? 0 : hP[i].as<float>();
      s_hours[s_hourN].code   = hC.isNull() ? 0 : hC[i].as<int>();
      s_hours[s_hourN].wind   = hW.isNull() ? 0 : hW[i].as<float>();
      s_hourN++;
    }
  }

  // --- daily ---
  // Index 0 is today. The screen shows the days AFTER today, so start at 1 -
  // "today" is already covered by the hourly rows above it.
  JsonArrayConst dt  = doc["daily"]["time"];
  JsonArrayConst dC  = doc["daily"]["weather_code"];
  JsonArrayConst dMx = doc["daily"]["temperature_2m_max"];
  JsonArrayConst dMn = doc["daily"]["temperature_2m_min"];
  JsonArrayConst dP  = doc["daily"]["precipitation_sum"];
  JsonArrayConst dW  = doc["daily"]["wind_speed_10m_max"];
  JsonArrayConst dSr = doc["daily"]["sunrise"];
  JsonArrayConst dSs = doc["daily"]["sunset"];
  s_dayN = 0;
  if (!dt.isNull()) {
    if (!dSr.isNull() && dSr.size() > 0) s_sunrise = (time_t)dSr[0].as<long long>();
    if (!dSs.isNull() && dSs.size() > 0) s_sunset  = (time_t)dSs[0].as<long long>();
    for (size_t i = 1; i < dt.size() && s_dayN < FORECAST_DAYS; i++) {
      s_days[s_dayN].t      = (time_t)dt[i].as<long long>();
      s_days[s_dayN].code   = dC.isNull()  ? 0 : dC[i].as<int>();
      s_days[s_dayN].tmax   = dMx.isNull() ? 0 : dMx[i].as<float>();
      s_days[s_dayN].tmin   = dMn.isNull() ? 0 : dMn[i].as<float>();
      s_days[s_dayN].precip = dP.isNull()  ? 0 : dP[i].as<float>();
      s_days[s_dayN].wind   = dW.isNull()  ? 0 : dW[i].as<float>();
      s_days[s_dayN].sunrise = dSr.isNull() ? 0 : (time_t)dSr[i].as<long long>();
      s_days[s_dayN].sunset  = dSs.isNull() ? 0 : (time_t)dSs[i].as<long long>();
      s_dayN++;
    }
  }

  s_valid = (s_hourN > 0 || s_dayN > 0);
  Serial.printf("Predpoved: %d hodin, %d dnu, vychod/zapad %s\n",
                s_hourN, s_dayN, (s_sunrise && s_sunset) ? "ok" : "chybi");
  Status_Set(ST_FORECAST, "OK, %d h / %d d", s_hourN, s_dayN);
  return s_valid;
}

// -----------------------------------------------------------------------------
//  Air quality
// -----------------------------------------------------------------------------
static bool fetchAirQuality() {
  char url[320];
  snprintf(url, sizeof(url),
    "%s?latitude=%.4f&longitude=%.4f"
    "&current=european_aqi,pm2_5,pm10,alder_pollen,birch_pollen,grass_pollen"
    "&timeformat=unixtime&timezone=UTC",
    AIRQUALITY_URL, Settings_Lat(), Settings_Lon());

  String body;
  if (!Net_GetString(url, body, "OVZDUSI")) return false;

  JsonDocument filter;
  JsonObject fc = filter["current"].to<JsonObject>();
  fc["european_aqi"] = true; fc["pm2_5"] = true; fc["pm10"] = true;
  fc["alder_pollen"] = true; fc["birch_pollen"] = true; fc["grass_pollen"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body,
                                             DeserializationOption::Filter(filter));
  body = String();
  if (err) { Serial.printf("OVZDUSI: JSON %s\n", err.c_str()); return false; }

  JsonObjectConst cur = doc["current"];
  if (cur.isNull()) return false;

  s_aqi  = cur["european_aqi"].isNull() ? -1 : cur["european_aqi"].as<int>();
  s_pm25 = cur["pm2_5"] | 0.0f;
  s_pm10 = cur["pm10"]  | 0.0f;

  // Pollen is a European-only product and reads zero out of season, so "no
  // data" and "no pollen" have to stay distinguishable: a missing key means we
  // show nothing at all rather than a confident zero.
  s_pollen = -1;
  s_pollenWorst = "";
  // Nazev se kresli na displej, takze bez diakritiky (font je 7bitove ASCII).
  struct { const char* key; const char* name; } P[] = {
    { "alder_pollen", "olse"  },
    { "birch_pollen", "briza" },
    { "grass_pollen", "travy" },
  };
  for (auto& p : P) {
    JsonVariantConst v = cur[p.key];
    if (v.isNull()) continue;
    float f = v.as<float>();
    if (f > s_pollen) {
      s_pollen = f;
      s_pollenWorst = p.name;
    }
  }

  s_aqOk = (s_aqi >= 0) || (s_pm25 > 0) || (s_pm10 > 0);
  Serial.printf("Ovzdusi: AQI %d, PM2.5 %.1f, pyl %.0f\n", s_aqi, s_pm25, s_pollen);
  return s_aqOk;
}

bool  AirQuality_Valid()  { return s_aqOk; }
int   AirQuality_Aqi()    { return s_aqi; }
float AirQuality_Pm25()   { return s_pm25; }
float AirQuality_Pm10()   { return s_pm10; }
float AirQuality_PollenMax() { return s_pollen; }
const char* AirQuality_PollenWorst() { return s_pollenWorst; }

// -----------------------------------------------------------------------------
void Forecast_Tick() {
  // No link: do not start the timer either. An attempt made while the WiFi was
  // still coming up would otherwise count as "tried" and push the next one half
  // an hour out - which is exactly why the temperature used to look broken.
  if (WiFi.status() != WL_CONNECTED) return;

  unsigned long now = millis();

  if (s_forceNext) { s_everTried = false; s_aqEverTried = false; s_forceNext = false; }

  if (!s_everTried || now - s_lastTry >= (s_valid ? FORECAST_PERIOD_MS : FORECAST_RETRY_MS)) {
    s_everTried = true;
    s_lastTry = now;
    fetchForecast();
    return;                    // one request per tick - never two back to back
  }

  if (!s_aqEverTried || now - s_aqLastTry >= (s_aqOk ? AQ_PERIOD_MS : AQ_RETRY_MS)) {
    s_aqEverTried = true;
    s_aqLastTry = now;
    fetchAirQuality();
  }
}
