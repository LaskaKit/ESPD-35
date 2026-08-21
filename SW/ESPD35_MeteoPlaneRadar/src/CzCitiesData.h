// MeteoPlaneRadar - vyvoj / development: chiptron.cz
// =============================================================================
//  Ceska mesta - vlastni seznam.
//
//  Evropska data (EuMapData.h) obsahuji jen sidla nad 50 000 obyvatel a jejich
//  zkratky se generuji strojove prvnimi ctyrmi pismeny, takze z Prahy je PRAH
//  a z Ostravy OSTR. Nad Ceskem je ale displej nejcasteji, a tady se hodi
//  zkratky, ktere clovek pozna: PHA, OVA, PLZ.
//
//  Tenhle seznam proto pro Ceskou republiku evropska data NAHRAZUJE. Mesto
//  z evropske sady, jehoz nazev je i tady, se preskoci (viz EuBorder.cpp), aby
//  se nic nekreslilo dvakrat. Zbytek Evropy zustava beze zmeny.
//
//  Dlouhe nazvy s "nad" jsou zkracene (Jablonec n. N., Zdar n. S., Usti n. L.),
//  aby se popisek vesel vedle tecky mesta i pri plnych nazvech.
//
//  59 mest, asi 1180 B ve flash.
//  Zdroj: rucne udrzovany seznam puvodni ceske mapy projektu.
// =============================================================================
#pragma once
#include "EuMapData.h"   // struct EuCity

// Obdelnik, ve kterem se evropska mesta nahrazuji temito. Zamerne tesne kolem
// hranic - Drazdany (51.05 N) lezi jen kousek nad nim a musi zustat evropske.
#define CZ_BOX_LAT0 48.55f
#define CZ_BOX_LAT1 51.06f
#define CZ_BOX_LON0 12.09f
#define CZ_BOX_LON1 18.87f

static const EuCity CZ_CITIES[] = {
  {"Praha", "PHA", 14.4378f, 50.0755f, 1},
  {"Brno", "BRNO", 16.6068f, 49.1951f, 1},
  {"Ostrava", "OVA", 18.2625f, 49.8209f, 1},
  {"Plzen", "PLZ", 13.3776f, 49.7384f, 1},
  {"Liberec", "LBC", 15.0543f, 50.7663f, 1},
  {"Olomouc", "OLO", 17.2509f, 49.5938f, 1},
  {"Ceske Budejovice", "CB", 14.4747f, 48.9747f, 1},
  {"Hradec Kralove", "HK", 15.8327f, 50.2092f, 1},
  {"Usti n. L.", "UNL", 14.0417f, 50.6607f, 1},
  {"Pardubice", "PCE", 15.7812f, 50.0343f, 1},
  {"Zlin", "ZLN", 17.6707f, 49.2265f, 1},
  {"Jihlava", "JIH", 15.5906f, 49.3961f, 1},
  {"Karlovy Vary", "KV", 12.8712f, 50.2306f, 1},
  {"Kladno", "KLD", 14.1017f, 50.1477f, 2},
  {"Most", "MOST", 13.6363f, 50.5031f, 2},
  {"Opava", "OPA", 17.9019f, 49.9387f, 2},
  {"Frydek-Mistek", "FM", 18.3505f, 49.6833f, 2},
  {"Karvina", "KAR", 18.5419f, 49.8540f, 2},
  {"Havirov", "HAV", 18.4364f, 49.7798f, 2},
  {"Teplice", "TEP", 13.8245f, 50.6404f, 2},
  {"Decin", "DEC", 14.2145f, 50.7821f, 2},
  {"Chomutov", "CHO", 13.4178f, 50.4605f, 2},
  {"Jablonec n. N.", "JBC", 15.1712f, 50.7243f, 2},
  {"Mlada Boleslav", "MB", 14.9038f, 50.4114f, 2},
  {"Prostejov", "PROS", 17.1118f, 49.4720f, 2},
  {"Prerov", "PRER", 17.4509f, 49.4554f, 2},
  {"Trebic", "TRB", 15.8814f, 49.2149f, 2},
  {"Ceska Lipa", "CL", 14.5376f, 50.6855f, 2},
  {"Trinec", "TRI", 18.6708f, 49.6776f, 2},
  {"Tabor", "TAB", 14.6578f, 49.4144f, 2},
  {"Znojmo", "ZNO", 16.0488f, 48.8555f, 2},
  {"Pribram", "PRI", 14.0104f, 49.6899f, 2},
  {"Cheb", "CHEB", 12.3740f, 50.0796f, 2},
  {"Trutnov", "TRU", 15.9124f, 50.5606f, 2},
  {"Kolin", "KOL", 15.2003f, 50.0281f, 2},
  {"Pisek", "PIS", 14.1476f, 49.3088f, 2},
  {"Kromeriz", "KRO", 17.3928f, 49.2979f, 2},
  {"Sumperk", "SUM", 16.9708f, 49.9653f, 2},
  {"Vsetin", "VSE", 17.9963f, 49.3387f, 2},
  {"Litomerice", "LTM", 14.1319f, 50.5344f, 2},
  {"Havlickuv Brod", "HB", 15.5800f, 49.6077f, 2},
  {"Strakonice", "STR", 13.9022f, 49.2619f, 2},
  {"Klatovy", "KLA", 13.2937f, 49.3955f, 2},
  {"Nachod", "NAC", 16.1655f, 50.4145f, 2},
  {"Zdar n. S.", "ZDS", 15.9394f, 49.5628f, 2},
  {"Uherske Hradiste", "UH", 17.4597f, 49.0698f, 2},
  {"Hodonin", "HOD", 17.1300f, 48.8489f, 2},
  {"Breclav", "BRE", 16.8820f, 48.7589f, 2},
  {"Jindrichuv Hradec", "JH", 15.0030f, 49.1442f, 2},
  {"Sokolov", "SOK", 12.6400f, 50.1814f, 2},
  {"Bruntal", "BRU", 17.4647f, 49.9884f, 2},
  {"Krnov", "KRN", 17.7047f, 50.0899f, 2},
  {"Vyskov", "VYS", 16.9989f, 49.2775f, 2},
  {"Blansko", "BLA", 16.6440f, 49.3630f, 2},
  {"Beroun", "BER", 14.0720f, 49.9639f, 2},
  {"Melnik", "MEL", 14.4740f, 50.3505f, 2},
  {"Nymburk", "NYM", 15.0414f, 50.1861f, 2},
  {"Ceska Trebova", "CT", 16.4478f, 49.9038f, 2},
  {"Svitavy", "SVI", 16.4682f, 49.7554f, 2},
};
static const int CZ_CITY_COUNT = 59;
