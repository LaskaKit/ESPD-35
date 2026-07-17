// =============================================================================
//  ESPD35_MeteoPlaneRadar - WiFi pripojeni + konfiguracni AP portal (rozhrani).
// =============================================================================
#pragma once
#include <Arduino.h>

#define AP_SSID     "ESPD35_MeteoPlaneRadar-Setup"
#define AP_PASSWORD ""            // prazdne = otevrena sit

bool   WiFi_ConnectOrPortal();    // pripoji se, nebo spusti portal (blokujici)
void   WiFi_StartPortal();        // rucne spusti portal (blokujici)
void   WiFi_Loop();               // udrzuje spojeni (volat v loop())
bool   WiFi_IsConnected();
String WiFi_SSID();
String WiFi_IP();
void   WiFi_Reset();              // smaze ulozene WiFi udaje
