// =============================================================================
//  ESPD35_MeteoPlaneRadar - obrazovka predpovedi (rozhrani).
//
//  Nahore nejblizsi hodiny, pod nimi nejblizsi dny, uplne dole radek ovzdusi.
//  Vsechno z Open-Meteo: zdarma, bez klice, bez registrace.
//
//  Na sirku je tahle obrazovka SNAZSI nez na kulatem displeji zdrojoveho
//  projektu: 480 / FORECAST_HOURS = presne 80 px na sloupec a kazdy radek ma
//  celou sirku, misto aby se u okraje zuzoval.
// =============================================================================
#pragma once
#include <Arduino.h>

void ScreenForecast_Enter();
void ScreenForecast_Draw();
bool ScreenForecast_Tick();     // true = prekreslit
