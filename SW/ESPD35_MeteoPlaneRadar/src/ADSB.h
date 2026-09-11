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
  //
  // Od 0.4.0 se pole jmenuje "hex", ne "icao" - stejne jako klic v odpovedi
  // adsb.fi a stejne jako ve zdrojovem projektu petus/MeteoPlaneRadar, aby
  // slo prenaset kod mezi obema bez tichych zamen.
  char  hex[8] = "";
  char  callsign[10] = "";
  char  type[10] = "";        // typ letadla (napr. A320), pokud dostupny
  // Registrace ("OK-TVU"), z pole "r" tehoz stazeni. Vozi se zadarmo v datech,
  // ktera stahujeme tak jako tak - proto uz neni potreba se na drak ptat druhe
  // API. Nejdelsi realne registrace maji deset znaku, jedenact plus koncova
  // nula tedy bohate staci.
  char  reg[12] = "";
  // Kod odpovidace jako ctyri osmickove cislice. Drzi se jako TEXT, ne cislo:
  // 7700 je osmickovy kod a "0021" nesmi zdegenerovat na 21.
  char  squawk[6] = "";
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
// -1, kdyz v poslednich datech neni (opustilo oblast, neveslo se do ADSB_MAX,
// nebo ho odfiltrovalo nastaveni). Pouzivat misto drzeni indexu mezi
// stazenimi - seznam se pokazde stavi znovu a poradi neni zarucene.
int    ADSB_FindByHex(const char* hex);

// Hlasi tohle letadlo nouzovy kod odpovidace?
//   7500 protipravni cin (unos), 7600 vypadek radia, 7700 obecna nouze
// Vraci ten kod, nebo nullptr, kdyz o zadny z nich nejde.
const char* ADSB_EmergencyCode(const Aircraft& a);

// Kolik letadel poslední stazeni odfiltrovalo podle nastaveni (vyskove pasmo,
// jen s volacim znakem). Pro stavovou stranku - aby prazdny radar sel odlisit
// od radaru s prisnym filtrem.
int    ADSB_FilteredOut();
