// =============================================================================
//  ESPD35_MeteoPlaneRadar - obrazovka radaru letadel (rozhrani).
//  Layout: 3/4 mapa vlevo + 1/4 detailovy panel vpravo (nejblizsi letadlo).
// =============================================================================
#pragma once
#include <Arduino.h>

void ScreenPlanes_Enter();       // inicializace pri startu
bool ScreenPlanes_Tick();        // stahovani dat; vraci true = prekreslit
void ScreenPlanes_Draw();        // vykresleni celeho snimku do canvasu

void ScreenPlanes_NextRange();     // kratky stisk tlacitka: dalsi rozsah
void ScreenPlanes_ToggleUnits();   // dlouhy stisk: letecke <-> metricke jednotky

// --- Dotykove ovladani ---
void ScreenPlanes_ChangeRange(int dir);  // swipe: +1 dalsi / -1 predchozi rozsah
// Kratky dotyk (tap) na souradnici obrazovky:
//   - na letadlo v mape  -> zafixuje ho v detailu (misto automaticky nejblizsiho)
//   - do prazdne mapy    -> zpet na automaticky nejblizsi
//   - do praveho panelu  -> prepne jednotky
// Vraci true, kdyz je potreba prekreslit.
bool ScreenPlanes_HandleTap(int x, int y);

// Zadost o WiFi/AP portal z tlacitka v panelu (obsluha v .ino, je blokujici).
bool ScreenPlanes_WantsPortal();
void ScreenPlanes_ClearPortal();
