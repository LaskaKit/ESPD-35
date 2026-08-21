// =============================================================================
//  ESPD35_MeteoPlaneRadar - venkovni teplota a stavovy radek s casem.
//
//  Rozdil proti zdrojovemu projektu petus/MeteoPlaneRadar: tam si tenhle modul
//  stahoval teplotu SAM a byl zaroven jedinym zdrojem casu. Tady jsou obe veci
//  jinde a Outside uz jen skládá vysledek:
//
//    CAS  - jde z NTP, se zalohou v hlavicce HTTP "Date" (viz Clock.h).
//    TEPLOTA - prijde v teze odpovedi jako predpoved (Forecast), takze si ji
//              modul nemusi stahovat podruhe. Forecast ji sem preda pres
//              Outside_NoteTemp().
//
//  Zustava tedy jedina odpovednost: dat dohromady kratky retezec
//  "21:42  18 degC" pro radek na obrazovkach - a poctive vynechat tu pulku,
//  ktera zatim neni znama.
// =============================================================================
#pragma once
#include <Arduino.h>

// Jsou hodiny nastavene? (Jen jineho jmeno pro Clock_Valid(), aby prenesene
// moduly z 0.6.1 nemusely vedet, ze se to tady jmenuje jinak.)
bool Outside_TimeValid();

// Predat surovou hodnotu hlavicky HTTP "Date". Prochazi rovnou do Clock.
void Outside_NoteHttpDate(const char* date);

// Forecast stahuje aktualni teplotu jako soucast sveho pozadavku a predava ji
// sem, misto aby se totez cislo stahovalo dvakrat.
void Outside_NoteTemp(float degC);

bool  Outside_TempValid();
float Outside_Temp();

// Hotovy radek, napr. "21:42  18 degC". Prazdny, dokud neni znamo nic;
// ukazuje jen tu pulku, ktera znama JE. Nikdy delsi nez OUTSIDE_TEXT_MAX-1.
#define OUTSIDE_TEXT_MAX 20
void Outside_StatusText(char* buf, size_t cap);
