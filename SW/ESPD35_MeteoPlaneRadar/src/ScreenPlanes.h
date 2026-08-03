// =============================================================================
//  ESPD35_MeteoPlaneRadar - obrazovka radaru letadel (rozhrani).
//  Layout: 3/4 mapa vlevo + 1/4 detailovy panel vpravo.
//
//  Tlacitka (jednotky, WiFi/poloha) se presunula na samostatnou obrazovku
//  Nastaveni - panel je diky tomu cely pro detail letadla.
// =============================================================================
#pragma once
#include <Arduino.h>

void ScreenPlanes_Enter();       // vstup na obrazovku (obnovi rozsah z NVS)
bool ScreenPlanes_Tick();        // stahovani dat; vraci true = prekreslit
void ScreenPlanes_Draw();        // vykresleni celeho snimku do canvasu

void ScreenPlanes_NextRange();     // kratky stisk tlacitka: dalsi rozsah
void ScreenPlanes_ToggleUnits();   // prepnuti letecke <-> metricke jednotky

// --- Dotykove ovladani ---
void ScreenPlanes_ChangeRange(int dir);  // swipe: +1 dalsi / -1 predchozi rozsah

// Kratky dotyk (tap) na souradnici obrazovky:
//   - na letadlo v mape  -> zafixuje ho v detailu (misto automaticky nejblizsiho)
//   - do prazdne mapy    -> zpet na automaticky nejblizsi
//   - do praveho panelu  -> take zpet na automaticky nejblizsi
// Vraci true, kdyz je potreba prekreslit.
bool ScreenPlanes_HandleTap(int x, int y);

// Je rucne zafixovane letadlo? (Swipe / dlouhy stisk pak nejdriv zrusi vyber.)
bool ScreenPlanes_DetailOpen();
void ScreenPlanes_CloseDetail();
