// =============================================================================
//  ESPD35_MeteoPlaneRadar - ADS-B klient (struktura Aircraft, stahovani z adsb.fi).
//  Limit ADSB_MAX je v Config.h.
// =============================================================================
#pragma once
#include <Arduino.h>
#include "Config.h"   // ADSB_MAX

#define ADSB_API_BASE "https://opendata.adsb.fi/api/v3/lat/"

struct Aircraft {
  float lat = 0;
  float lon = 0;
  float track = 0;            // smer letu (stupne)
  float altFt = 0;            // vyska v stopach (baro)
  float gsKt = 0;             // rychlost nad zemi (uzly)
  float baroRate = 0;         // stoupani/klesani (ft/min)
  // ICAO 24bitova adresa, napr. "49d0d1". Je to IDENTITA letadla - je vazana
  // na drak a mezi stazenimi se nemeni, na rozdil od pozice v seznamu, kterou
  // adsb.fi muze libovolne prehazet. Vyber v ScreenPlanes se proto drzi tohohle,
  // nikdy ne indexu v poli.
  // 8 bajtu: adsb.fi pred neICAO cile (TIS-B/ADS-R) pisi '~', tedy 7 znaku
  // plus ukoncovaci nula.
  char  icao[8] = "";
  char  callsign[10] = "";
  char  type[10] = "";        // typ letadla (napr. A320), pokud dostupny
  bool  onGround = false;     // vzdy false - pozemni letadla se uz nezarazuji
  bool  hasTrack = false;     // false = smer neznamy (kresli se jinak)
};

void   ADSB_SetPollFn(void (*fn)());

// Stahne letadla v okruhu radiusKm kolem polohy. Vraci true pri uspechu.
// Pri neuspechu zustane posledni PLATNY snimek - radar se nikdy nevymaze.
bool   ADSB_Fetch(double lat, double lon, float radiusKm);

int    ADSB_Count();
const Aircraft* ADSB_List();

// Dohleda letadlo podle ICAO hex adresy. Vraci aktualni index v seznamu, nebo
// -1, kdyz v poslednich datech neni (opustilo oblast, nebo se neveslo do
// ADSB_MAX). Pouzivat misto drzeni indexu mezi stazenimi - seznam se pokazde
// staví znovu a poradi neni zarucene.
int    ADSB_FindByIcao(const char* icao);
