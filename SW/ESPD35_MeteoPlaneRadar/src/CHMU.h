// =============================================================================
//  ESPD35_MeteoPlaneRadar - meteoradar CHMU: stahovani srazkoveho kompozitu.
//  Rozhrani + kalibrace geografickych okraju + animace (vice ramcu).
// =============================================================================
#pragma once
#include <Arduino.h>

#define CHMU_INDEX_URL        "https://opendata.chmi.cz/meteorology/weather/radar/composite/maxz/png/"
// Tentyz kompozit porovnany s merenim ve 2 km (0.8.0): plnou barvou jen
// srazky, ktere dopadaji na zem.
#define CHMU_INDEX_URL_MASKED "https://opendata.chmi.cz/meteorology/weather/radar/composite/maxz/png_masked/"

// Geograficke ohraniceni CELEHO obrazku PNG (dle dokumentace CHMU).
//   z.d. 11,267 - 20,770 ; z.s. 48,047 - 52,167
#define CHMU_LON_LEFT   11.267f
#define CHMU_LON_RIGHT  20.770f
#define CHMU_LAT_TOP    52.167f
#define CHMU_LAT_BOTTOM 48.047f

// Ohraniceni skutecnych DAT (uzsi nez cely obrazek). Za timto rozsahem je v PNG
// vlevo? ne - nahore titulek "CZRAD - Z: MAX ..." a vpravo barevna skala/okraj.
// Tyto oblasti maskujeme na cerno, aby se nezobrazovaly jako srazky.
#define CHMU_LON_DATA_RIGHT 19.624f
#define CHMU_LAT_DATA_TOP   51.458f

#define CHMU_MAX_PNG 131072      // max velikost jednoho PNG (~15-30 kB, rezerva)
#define CHMU_ANIM_MAX 6          // max poctu ramcu animace

void        CHMU_SetPollFn(void (*fn)());

// Zdroj: cely kompozit, nebo "masked" (0.8.0). Plati pro animaci.
void        CHMU_SetMasked(bool masked);
bool        CHMU_Masked();

// Jediny nejnovejsi snimek do bufferu volajiciho - pro hlidani bourky.
// name dostane nazev souboru (z nej cas snimku).
bool        CHMU_FetchLatest(bool masked, uint8_t* buf, size_t cap, size_t* outSize, String* name);
String      CHMU_AnimName(int i);                        // nazev souboru ramce i
String      CHMU_TimeTextFromName(const String& name);   // HH:MM mistniho casu

// --- Jeden (nejnovejsi) snimek ---

// --- Animace: nejnovejsich wantN ramcu (5 min krok) ---
// Vraci pocet stazenych ramcu. Ramce jsou serazene 0 = nejstarsi ... N-1 = nyni.
int         CHMU_FetchAnim(int wantN);
int         CHMU_AnimCount();
uint8_t*    CHMU_AnimData(int i);
size_t      CHMU_AnimSize(int i);
String      CHMU_AnimTimeText(int i);   // HH:MM (lokalni cas snimku)
