// =============================================================================
//  ESPD35_MeteoPlaneRadar - cas z hlavicky HTTP "Date" jako ZALOHA k NTP.
//
//  Proc to vubec je: NTP jede po UDP portu 123, ktery nekteri poskytovatele
//  blokuji. Kazda HTTPS odpoved, kterou stejne stahujeme (adsb.fi kazdych
//  5-15 s, CHMU kazdych 5 min), ale nese hlavicku "Date:" v GMT s presnosti
//  na sekundu. Je to tedy cas zadarmo, bez dalsiho spojeni a bez dalsiho portu.
//
//  DULEZITE: NTP ma PREDNOST. Hlavicka Date ma rozliseni jen na sekundy a je
//  posunuta o dobu prenosu, takze prepisovat ji spravne nastavene hodiny by
//  znamenalo, ze cas kazdych par sekund poskoci. Nize se proto zapisuje jen
//  tehdy, kdyz systemove hodiny jeste nejsou platne.
//
//  (Zdrojovy projekt petus/MeteoPlaneRadar NTP nepouziva vubec a nastavuje cas
//  z kazde odpovedi - tady je to zamerne obracene.)
// =============================================================================
#pragma once
#include <Arduino.h>

// Predej surovou hodnotu hlavicky "Date", napr.
// "Sun, 09 Aug 2026 20:00:56 GMT". Cokoli neprevoditelneho se ignoruje.
// Vola se po kazdem uspesnem pozadavku (ADSB.cpp, CHMU.cpp, Net.cpp).
void Clock_NoteHttpDate(const char* date);

// Maji systemove hodiny pouzitelny cas? (Z NTP nebo z hlavicky Date.)
bool Clock_Valid();

// Odkud cas prisel - pro seriovy log a pozdeji pro stavovou stranku.
// "NTP", "HTTP Date" nebo "-".
const char* Clock_Source();
