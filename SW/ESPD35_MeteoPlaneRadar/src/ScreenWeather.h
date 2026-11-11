// =============================================================================
//  ESPD35_MeteoPlaneRadar - obrazovka meteoradaru CHMU (rozhrani).
//  Celoobrazovkovy vyrez srazkoveho kompozitu 480x320 se spravnym (izotropnim)
//  meritkem a Web Mercator projekci; obrys CR + mesta jako podkres.
// =============================================================================
#pragma once
#include <Arduino.h>

void ScreenWeather_Enter();
bool ScreenWeather_Tick();          // stahovani; true = prekreslit
void ScreenWeather_Draw();

void ScreenWeather_ChangeRange(int dir);   // swipe / tlacitko: zmena rozsahu

// Zmenil se zdroj (CHMU / CHMU masked, 0.8.0): zahodit animaci, pri pristim
// zobrazeni se stahne znovu.
void ScreenWeather_SourceChanged();
