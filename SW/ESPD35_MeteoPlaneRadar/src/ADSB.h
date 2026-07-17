// =============================================================================
//  ESPD35_MeteoPlaneRadar - ADS-B klient (struktura Aircraft, stahovani z adsb.fi).
//  Prevzato beze zmeny z petus/Meteo-PlaneRadar.
// =============================================================================
#pragma once
#include <Arduino.h>

#define ADSB_API_BASE "https://opendata.adsb.fi/api/v3/lat/"
#define ADSB_MAX 40           // strop letadel k vykresleni

struct Aircraft {
  float lat = 0;
  float lon = 0;
  float track = 0;            // smer letu (stupne)
  float altFt = 0;            // vyska v stopach (baro)
  float gsKt = 0;             // rychlost nad zemi (uzly)
  float baroRate = 0;         // stoupani/klesani (ft/min)
  char  icao[7] = "";         // ICAO hex adresa - stabilni ID letadla
  char  callsign[10] = "";
  char  type[10] = "";        // typ letadla (napr. A320), pokud dostupny
  bool  onGround = false;
  bool  hasTrack = false;     // false = smer neznamy (kresli se jinak)
};

void   ADSB_SetPollFn(void (*fn)());

// Stahne letadla v okruhu radiusKm kolem polohy. Vraci true pri uspechu.
bool   ADSB_Fetch(double lat, double lon, float radiusKm);

int    ADSB_Count();
const Aircraft* ADSB_List();
