// =============================================================================
//  ESPD35_MeteoPlaneRadar - obrazovka hodin (rozhrani).
//
//  Velky digitalni cas, datum, aktualni pocasi a beh sekund po obvodu ramu.
//  Vsechno, co ukazuje, uz deska stahuje kvuli necemu jinemu (cas z NTP
//  a z hlavicek HTTP, pocasi z teze odpovedi jako predpoved), takze tahle
//  obrazovka NEPRIDAVA ani jeden pozadavek navic.
//
//  Prstenec sekund ze zdrojoveho projektu (kulaty displej 480x480) se na
//  obdelnik nevejde. Nejblizsi nahradou, ktera zachova puvodni myslenku, je
//  BEH PO OBVODU: 60 pozic rozdelenych po ramu 480x320, tedy 1600 px obvodu
//  deleno 60 = 26,7 px na sekundu.
// =============================================================================
#pragma once
#include <Arduino.h>

void ScreenClock_Enter();
void ScreenClock_Draw();
bool ScreenClock_Tick();                  // true = prekreslit

// Kratke klepnuti prepina den/noc, ale JEN kdyz je automatika vypnuta -
// jinak by ji automatika za chvili stejne vratila zpet a tlacitko by
// vypadalo rozbite.
bool ScreenClock_HandleTap(int x, int y);
