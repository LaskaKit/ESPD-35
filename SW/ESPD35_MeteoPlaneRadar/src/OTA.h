// =============================================================================
//  ESPD35_MeteoPlaneRadar - aktualizace firmwaru pres WiFi (rozhrani).
// =============================================================================
#pragma once
#include <Arduino.h>

// Spusti rezim OTA: otevre AP, ukaze QR kod a ceka na nahrani firmwaru
// z prohlizece na 192.168.4.1/update.
//
// BLOKUJICI. Konci restartem desky - at uz po uspesne aktualizaci (restartuje
// ElegantOTA), nebo po klepnuti na displej / vyprseni OTA_IDLE_MS.
void OTA_Run();
