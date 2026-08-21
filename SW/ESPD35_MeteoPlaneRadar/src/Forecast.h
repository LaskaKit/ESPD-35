// =============================================================================
//  ESPD35_MeteoPlaneRadar
//  Weather forecast, sunrise/sunset and air quality - all from Open-Meteo.
//  Free, no key, no registration.
//
//  ONE request serves three features, which is why they share a module:
//    - the forecast screen (next hours + next days),
//    - the current temperature and precipitation on the clock screen,
//    - sunrise and sunset, which drive the automatic night mode.
//
//  Times are requested as plain UTC epochs (timezone=UTC, timeformat=unixtime)
//  rather than local ones. Our own clock is a UTC epoch too (seeded from the
//  HTTP Date header), so everything can be compared directly and localtime_r()
//  does the conversion for display exactly once. Asking Open-Meteo for local
//  times as well is how you end up an hour out twice a year.
//
//  Prevzato z petus/MeteoPlaneRadar v0.6.1 (autor Petr / chiptron.cz)
//  a upraveno pro ESPD-3.5 (ILI9488 480x320, SPI).
// =============================================================================
#pragma once
#include <Arduino.h>
#include <time.h>
#include "Config.h"

struct FcHour {
  time_t t     = 0;
  float  temp  = 0;    // degC
  float  precip = 0;   // mm
  float  wind  = 0;    // km/h
  int    code  = 0;    // WMO weather code
};

struct FcDay {
  time_t t      = 0;
  float  tmax   = 0;
  float  tmin   = 0;
  float  precip = 0;
  float  wind   = 0;
  int    code   = 0;
  time_t sunrise = 0;
  time_t sunset  = 0;
};

// Call from loop(). Fetches when due, cheap no-op otherwise.
void Forecast_Tick();

// Ask for a refresh on the next tick (after the location changed).
void Forecast_Invalidate();

bool Forecast_Valid();
int  Forecast_HourCount();
const FcHour* Forecast_Hours();
int  Forecast_DayCount();
const FcDay*  Forecast_Days();

// Current conditions (the "current" block of the same response).
bool  Forecast_CurrentValid();
float Forecast_CurrentTemp();
float Forecast_CurrentPrecip();
float Forecast_CurrentWind();     // km/h
int   Forecast_CurrentCode();

// Today's sun times as UTC epochs. False when nothing has been fetched yet, in
// which case night mode has to fall back on the manual setting.
bool Forecast_SunTimes(time_t* rise, time_t* set);

// --- Air quality (separate endpoint, same idea) ------------------------------
bool  AirQuality_Valid();
int   AirQuality_Aqi();        // European AQI, -1 when unknown
float AirQuality_Pm25();
float AirQuality_Pm10();
// Nejvyssi ze tri hodnot pyloveho zpravodajstvi (grains/m3), or -1 outside Europe / out of
// season, where the API simply returns nothing.
float AirQuality_PollenMax();
const char* AirQuality_PollenWorst();   // "briza" apod., "" kdyz neni znamo
