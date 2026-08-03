// =============================================================================
//  ESPD35_MeteoPlaneRadar - ulozeni nastaveni do NVS (rozhrani).
//  Vychozi poloha je DEFAULT_LAT / DEFAULT_LON v Config.h.
// =============================================================================
#pragma once
#include <Arduino.h>
#include "Config.h"   // DEFAULT_LAT / DEFAULT_LON

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

// -----------------------------------------------------------------------------
//  Stav UI, ktery prezije restart: rozsah zvlast pro kazdou obrazovku +
//  naposledy zobrazena obrazovka + orientace mapy.
//
//  Zapisuje se ODLOZENE (viz Settings_Tick). Puvodni verze zapisovala do flash
//  pri KAZDE zmene rozsahu, takze rychle prejizdeni prstem primo opotrebovavalo
//  NVS. Nove se zmeny akumuluji a zapisou az po chvili klidu.
// -----------------------------------------------------------------------------
uint8_t Settings_PlaneRange();
void    Settings_SetPlaneRange(uint8_t idx);
uint8_t Settings_MeteoRange();
void    Settings_SetMeteoRange(uint8_t idx);
uint8_t Settings_Screen();
void    Settings_SetScreen(uint8_t idx);

// Ktery svetovy smer (azimut ve stupnich, 0..359) je NAHORE na radaru letadel.
// 0 = sever nahoru, 90 = divam se na vychod. Zadava se smer pohledu, ne
// "o kolik mapu otocit" - to je jina velicina a plete se (viz CHANGELOG).
uint16_t Settings_TopBearing();
void     Settings_SetTopBearing(uint16_t deg);

// Volat jednou za loop(). Zapise cekajici zmeny UI stavu do NVS po kratke
// dobe klidu, aby jeden swipe neznamenal jeden zapis do flash.
void    Settings_Tick();

void   Settings_ClearAll();
