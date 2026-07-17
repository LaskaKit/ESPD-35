// =============================================================================
//  ESPD35_MeteoPlaneRadar - hardwarovy watchdog pro provoz 24/7 (rozhrani).
// =============================================================================
#pragma once
#include <Arduino.h>

void Watchdog_Begin();     // spusti WDT a prihlasi loop task
void Watchdog_Feed();      // nakrmit (volat v loop())
void Watchdog_Suspend();   // docasne odhlasit (napr. behem blokujiciho portalu)
void Watchdog_Resume();    // znovu prihlasit
