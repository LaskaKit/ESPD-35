// =============================================================================
//  ESPD35_MeteoPlaneRadar - obrazovka: z ceho se prave vyrabi elektrina.
//
//  Funkce prevzata z petus/MeteoPlaneRadar 0.7.0 (autor Petr / chiptron.cz),
//  rozvrzeni nakreslene znovu pro obdelnik 480x320.
//
//  Vlevo prstenec podilu jednotlivych zdroju, uprostred podil obnovitelnych
//  zdroju. Vpravo legenda serazena podle podilu, pod ni spotreba a saldo
//  preshranicnich toku a cas vzorku - data ENTSO-E chodi se zpozdenim jedne az
//  dvou hodin, takze nesmi vypadat jako zive.
//
//  Zemi lze zvolit ve webovem nastaveni (zalozka Energie), vychozi je Cesko.
//  Obrazovka nema dotykove cile ani rozsah.
// =============================================================================
#pragma once
#include <Arduino.h>

void ScreenMix_Enter();
void ScreenMix_Draw();
bool ScreenMix_Tick();
