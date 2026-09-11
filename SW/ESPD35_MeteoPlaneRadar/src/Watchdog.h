// =============================================================================
//  ESPD35_MeteoPlaneRadar - hardwarovy watchdog pro provoz 24/7 (rozhrani).
//  Timeout je WDT_TIMEOUT_S v Config.h.
// =============================================================================
#pragma once
#include <Arduino.h>
#include "Config.h"   // WDT_TIMEOUT_S

void Watchdog_Begin();     // spusti WDT a prihlasi loop task
void Watchdog_Feed();      // nakrmit (volat v loop())

// POZN.: Watchdog_Suspend()/Resume() zaniklo ve verzi 0.5.0. Existovalo jen
// kvuli blokujicimu portalu WiFiManageru a blokujicimu rezimu ElegantOTA -
// obojí bezelo nekolik minut mimo loop() a watchdog nemel kdo krmit. Od 0.5.0
// nic hlavni smycku neblokuje (vlastni portal i nahravani firmwaru bezi
// z loop()), takze watchdog uz nemusi nikdy spat. To je samo o sobe zvyseni
// spolehlivosti: deska byla driv bez dohledu presne ve chvili, kdy s ni
// uzivatel pracoval.
