// =============================================================================
//  ESPD35_MeteoPlaneRadar - jednoradkove hlaseni o zdravi pro kazdy zdroj dat.
//
//  Diagnostika zatim znamenala pripojit USB kabel a sledovat seriovou linku.
//  Az bude bezet webova konfigurace (v0.5.0), je pro to lepsi misto: kazdy
//  stahovac tu nechá kratkou poznamku a stavova stranka je ukaze vedle sebe -
//  takze "meteoradar je prazdny" jde zodpovedet bez sahnuti na hardware.
//
//  Do te doby to slouzi aspon k tomu, ze stav je na jednom miste a da se
//  vypsat jednim volanim.
// =============================================================================
#pragma once
#include <Arduino.h>

enum StatusSlot : uint8_t { ST_ADSB = 0, ST_RADAR, ST_FORECAST, ST_COUNT };

// Jako printf; text se orizne na delku, kterou stranka zvladne zobrazit.
void Status_Set(StatusSlot slot, const char* fmt, ...);

// "OK, 12 letadel (pred 4 s)" - poznamka plus jak je stara.
void Status_Text(StatusSlot slot, char* out, size_t cap);

// Vypise vsechny sloty na seriovou linku (ladeni).
void Status_Dump();
