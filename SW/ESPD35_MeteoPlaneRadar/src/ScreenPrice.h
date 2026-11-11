// =============================================================================
//  ESPD35_MeteoPlaneRadar - obrazovka: spotova cena elektriny.
//
//  Funkce prevzata z petus/MeteoPlaneRadar 0.7.0 (autor Petr / chiptron.cz),
//  rozvrzeni nakreslene znovu pro obdelnik 480x320.
//
//  Vlevo je den jako prstenec: pulnoc nahore, po smeru hodinovych rucicek,
//  devadesat sest ctvrthodin obarvenych od zelene (levne) po cervenou (drahe).
//  Rucicka ukazuje na probihajici ctvrthodinu. Vpravo je velke cislo - cena
//  prave probihajici ctvrthodiny v Kc/kWh - a pod nim nejlevnejsi a nejdrazsi
//  blok dne.
//
//  Ovladani:
//    klepnuti na prstenec   - vypise cenu vybrane ctvrthodiny
//    klepnuti doprostred    - prepne dnes / zitra (zitrek vychazi kolem 14:00)
//      nebo na pravy panel
//    swipe                  - totez prepnuti (je to "rozsah" teto obrazovky)
// =============================================================================
#pragma once
#include <Arduino.h>

void ScreenPrice_Enter();
void ScreenPrice_Draw();
bool ScreenPrice_Tick();
bool ScreenPrice_HandleTap(int x, int y);

// Swipe: dnes <-> zitra. Jmenuje se jako rozsah radaru, aby s ni spravce
// obrazovek zachazel stejne.
void ScreenPrice_ChangeRange(int dir);
