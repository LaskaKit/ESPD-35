// =============================================================================
//  ESPD35_MeteoPlaneRadar
//  Day / night brightness, driven by the sun.
//
//  No light sensor and no home-automation server: sunrise and sunset arrive as
//  part of the forecast request we make anyway, so the automatic switch costs
//  nothing extra. The offset shifts BOTH events symmetrically - a positive
//  value makes the night start earlier and end later, which is what you want in
//  a room that goes dark before the sun is actually down.
//
//  Prevzato z petus/MeteoPlaneRadar v0.6.1 (autor Petr / chiptron.cz)
//  a upraveno pro ESPD-3.5 (ILI9488 480x320, SPI).
// =============================================================================
#pragma once
#include <Arduino.h>

// Call from loop(). Decides day/night and pushes the matching brightness.
void NightMode_Tick();

// Manual switch, used by a short tap on the clock screen when the automatic
// mode is off. Does nothing while automatic mode is on - it would be undone a
// second later anyway.
void NightMode_Toggle();

// Push the brightness for the current state to the panel. Call after the user
// changes a brightness value so the effect is immediate.
void NightMode_Apply();
