// =============================================================================
//  ESPD35_MeteoPlaneRadar - automaticka detekce polohy podle verejne IP (rozhrani).
// =============================================================================
#pragma once
#include <Arduino.h>

// Doplni polohu dle IP, jen kdyz ji uzivatel nezadal rucne. Vraci true, kdyz
// se poloha zjistila a ulozila.
bool GeoIP_DetectIfNeeded();
