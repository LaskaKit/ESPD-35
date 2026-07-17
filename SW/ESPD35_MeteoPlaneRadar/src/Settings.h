// =============================================================================
//  ESPD35_MeteoPlaneRadar - ulozeni nastaveni do NVS (rozhrani).
// =============================================================================
#pragma once
#include <Arduino.h>

// Vychozi poloha (Praha) - prepise se pri prvnim startu geolokaci nebo rucne.
#define DEFAULT_LAT 50.0755
#define DEFAULT_LON 14.4378

void   Settings_Begin();

double Settings_Lat();
double Settings_Lon();
bool   Settings_HasLocation();
void   Settings_SetLocation(double lat, double lon);

uint8_t Settings_Backlight();
void    Settings_SetBacklight(uint8_t pct);

// Jednotky v detailu letadla: false = letecke (ft/kt), true = metricke (m/kmh).
bool    Settings_MetricUnits();
void    Settings_SetMetricUnits(bool metric);

// Index zvoleneho rozsahu (prezije restart).
uint8_t Settings_RangeIdx();
void    Settings_SetRangeIdx(uint8_t idx);

void   Settings_ClearAll();
