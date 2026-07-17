// =============================================================================
//  ESPD35_MeteoPlaneRadar - kapacitni dotyk FT5436/FT6236 (I2C) - rozhrani.
//
//  Vyzaduje knihovnu FT6236 (DustinWatts, s upravenym CHIPID/VENDID pro FT5436),
//  kterou LaskaKit prikladá k desce ESPD-3.5. Pridano v tomto souboru.
// =============================================================================
#pragma once
#include <Arduino.h>

// Inicializace I2C sbernice a dotykoveho radice. Vraci true pri uspechu.
bool Touch_Init();

// Precte aktualni dotyk. Pri doteku naplni *x,*y (souradnice displeje) a vrati
// true. Bez doteku vrati false.
bool Touch_Read(int* x, int* y);
