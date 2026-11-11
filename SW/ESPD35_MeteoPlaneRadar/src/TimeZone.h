// =============================================================================
//  ESPD35_MeteoPlaneRadar - mistni casove pasmo.
//
//  Prevzato z petus/MeteoPlaneRadar 0.7.0 (autor Petr / chiptron.cz).
//
//  NTP i hlavicka Date davaji UTC, takze time() je spravna UTC epocha kdekoli
//  na svete. Prevod na MISTNI cas je jina vec a do teto verze ho resila
//  konstanta TZ_INFO v Config.h - spravne v Praze a spatne vsude jinde. Deska
//  odvezena do New Yorku ukazovala na hodinach cesky cas a predpoved
//  popisovala ceskymi hodinami: ne prazdna obrazovka, ale sebejiste spatna.
//
//  Posun ted chodi s predpovedi. Open-Meteo odpovida utc_offset_seconds pro
//  zadane souradnice, predpoved se stahuje kazdou pulhodinu tak jako tak
//  a nestoji to zadny dotaz navic.
//
//  PROC POSUN A NE NAZEV ZONY: newlib v ESP-IDF nema databazi IANA, takze
//  setenv("TZ", "America/New_York") nic uzitecneho neudela. TZ musi byt POSIX
//  retezec - bud cele pravidlo s daty letniho casu, nebo pevny posun. Pouziva
//  se posun; predpoved se obnovuje po tricet minutach, takze jarni a podzimni
//  zmena casu se srovna do pul hodiny.
//
//  Zkompilovane TZ_INFO se NEZAHAZUJE. Nese prava ceska pravidla letniho casu,
//  takze dokud se s nim posun ze site shoduje, zustava - ceska deska si
//  ponecha presna pravidla. Teprve neshoda prepne na pevny posun.
// =============================================================================
#pragma once
#include <Arduino.h>

// Nastavi vychozi TZ_INFO z Config.h. Volat jednou v setup(), driv nez cokoli
// prevadi cas k zobrazeni.
void TimeZone_Begin();

// Posun na vychod od UTC v sekundach, jak ho pro souradnice uzivatele hlasi
// predpoved. Kdyz se shoduje s tim, co aktivni pravidlo uz dava (bezny pripad
// v Cesku), nedela nic.
void TimeZone_SetOffset(int seconds);

// Co plati prave ted, v sekundach na vychod od UTC. Cte se zpet z knihovny C,
// takze plati i driv, nez se ozve sit.
int TimeZone_Offset();

// True, jakmile se opravdu pouzil posun ze site - tedy deska je nekde, kde
// zkompilovane pravidlo neplati. Ukazuje se na stavove strance.
bool TimeZone_FromNetwork();

// POSIX retezec, ktery je prave v TZ (stavova stranka, seriovy vypis).
const char* TimeZone_Text();
