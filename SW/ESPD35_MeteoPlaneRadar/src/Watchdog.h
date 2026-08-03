// =============================================================================
//  ESPD35_MeteoPlaneRadar - hardwarovy watchdog pro provoz 24/7 (rozhrani).
//  Timeout je WDT_TIMEOUT_S v Config.h.
// =============================================================================
#pragma once
#include <Arduino.h>
#include "Config.h"   // WDT_TIMEOUT_S

void Watchdog_Begin();     // spusti WDT a prihlasi loop task
void Watchdog_Feed();      // nakrmit (volat v loop())
void Watchdog_Suspend();   // docasne odhlasit (napr. behem blokujiciho portalu)
void Watchdog_Resume();    // znovu prihlasit
