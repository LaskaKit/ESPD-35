// =============================================================================
//  ESPD35_MeteoPlaneRadar - rozvrzeni obrazovky: sdilene pruhy a kontrola
//  prekryvu, aby se dva prvky nikdy nevykreslily pres sebe.
//
//  Obdelnikova varianta modulu z petus/MeteoPlaneRadar v0.6.1. Na kulatem
//  panelu resil hlavne ubyvajici sirku u okraje; tady ta cast odpada, ale
//  DRUHA polovina ma cenu i na 480x320 - obrazovek bude pet a vyska 320 px
//  je citelne mene nez tamnich 480.
//
//  Dva mechanismy:
//
//  1) PEVNE PRUHY (konstanty LY_*). Kazda obrazovka dava tentyz druh
//     informace na tentyz radek: hodiny vzdy na LY_STATUS, rozsah vzdy na
//     LY_RANGE. Dve obrazovky se tak nemuzou neshodnout na tom, kam co patri,
//     a Layout_SelfTest() dokaze, ze se pruhy neprekryvaji navzajem.
//
//  2) BEHOVE NAROKY. Pevne prvky se REZERVUJI pred kreslenim mapy. Cokoli
//     umisteneho podle DAT - nazvy mest, volaci znaky letadel - si pak musi
//     svuj obdelnik NAROKOVAT a kdyz je misto zabrane, nekresli se vubec.
//     Prednost je poradi kresleni: pulka volaciho znaku pod legendou je horsi
//     nez zadny volaci znak.
// =============================================================================
#pragma once
#include <Arduino.h>
#include "Config.h"

// --- Pevne prvky ------------------------------------------------------------
//
// y je HORNI okraj textu, tak jak s nim pracuje kurzor Arduino_GFX.
// Hodnoty odpovidaji tomu, co uz obrazovky kresli dnes - modul je tim padem
// jen pojmenovava, nic se vizualne nemeni.
//
// Vodorovny rozmer je tu proto, ze na 480x320 lezi nekolik prvku na TEMZE
// radku vedle sebe (rozsah vlevo dole, tecky obrazovek uprostred, podpis
// vpravo). Bez sirky by je self-test nize hlasil jako prekryv, ackoli se
// nepotkavaji.
#define LY_STATUS       4                  // stav / pocet letadel   (velikost 1)
#define LY_STATUS_X     4
#define LY_STATUS_W    92

#define LY_LEGEND      20                  // legenda letovych hladin (velikost 1)
#define LY_LEGEND_X     4
#define LY_LEGEND_W    66
#define LY_LEGEND_H    (12 + 6 * 13)       // vyska cele legendy vcetne nadpisu

// Popis rozsahu, napr. "100 km" - sest znaku velikosti 1 = 36 px, s rezervou 44.
#define LY_RANGE       (LCD_HEIGHT - 22)   // horni okraj textu (velikost 1)
#define LY_RANGE_X      4
#define LY_RANGE_W     44

// Tecky rozsahu: stred y = LCD_HEIGHT-8, polomer 3, ctyri tecky po 14 px
// od x=8. Obdelnik je tedy y 308..316 a x 4..54 - tesne POD popisem rozsahu,
// ne pres nej. (Prvni verze konstant tady mela prekryv osmi pixelu; odhalil
// ho self-test nize, presne k tomu je.)
#define LY_RANGE_DOTS  (LCD_HEIGHT - 8)    // STRED y
#define LY_RANGE_DOTS_X 4
#define LY_RANGE_DOTS_W 50
#define LY_RANGE_DOTS_Y0 (LY_RANGE_DOTS - 4)
#define LY_RANGE_DOTS_H 8

// Tecky prepinani obrazovek. Sirka se odvozuje od poctu obrazovek, ktery zna
// jen .ino - proto se sem predava pri rezervaci. Konstanta nize je nejhorsi
// pripad (pet obrazovek pri LY_DOTS_GAP), aby self-test kontroloval to, co
// nastane po rozsireni, ne jen dnesni stav.
#define LY_DOTS        308                 // STRED y
// Polomer tecky. Od 0.4.0 mensi (bylo 6) - indikator ma ukazovat, kde jste,
// ne poutat pozornost.
#define LY_DOTS_R        4
#define LY_DOTS_GAP     20
#define LY_DOTS_MAX_N    5
#define LY_DOTS_HALF   (LY_DOTS_GAP * LY_DOTS_MAX_N / 2)
#define LY_DOTS_X      (LCD_WIDTH / 2 - LY_DOTS_HALF)
#define LY_DOTS_W      (LY_DOTS_HALF * 2)
#define LY_DOTS_Y0     (LY_DOTS - 10)
#define LY_DOTS_H       18

// Spodni pruh praveho panelu. Do 0.4.0 tu byl podpis "laskakit.cz"; ten
// z obrazovky letadel zmizel, ale pas zustava rezervovany - je to jediny kus
// panelu, kam se nema nic umistovat automaticky.
#define LY_FOOTER      (LCD_HEIGHT - 12)
#define LY_FOOTER_X    (MAP_W + 6)
#define LY_FOOTER_W    66

// Metriky vestaveneho GFX fontu (bunka 6x8 px pri velikosti 1).
#define LY_CHAR_W(size) (6 * (size))
#define LY_CHAR_H(size) (8 * (size))

// Zacatek snimku - zahodi vsechny rezervace. Volat jednou, pred kreslenim.
void Layout_Begin();

// Pevny prvek: zabere misto bez ohledu na to, jestli tam uz neco je. Pevne
// prvky jsou umistene navrhem a vyhravaji vzdy; pravidlo poradi je, ze se
// rezervuji driv, nez cokoli umistene daty dostane sanci narokovat.
void Layout_Reserve(int x, int y, int w, int h);

// Rezervuje vodorovny pruh pres celou sirku displeje. Pro cokoli
// vycentrovaneho, co se tahne pres obrazovku.
void Layout_ReserveBand(int y, int h);

// Rezervuje presne to, co zabere vycentrovany retezec.
void Layout_ReserveTextCentered(const char* s, uint8_t size, int cx, int y);

// Prvek umisteny daty: zabere misto, jen kdyz je volne.
// Vraci false, kdyz neni - volajici pak nekresli vubec nic.
bool Layout_Claim(int x, int y, int w, int h);

// Test bez zabrani.
bool Layout_IsFree(int x, int y, int w, int h);

// Sirka retezce ve vestavenem fontu.
int  Layout_TextW(const char* s, uint8_t size);

// Vejde se cely obdelnik na displej?
bool Layout_OnScreen(int x, int y, int w, int h);

// Kolik mista ma radek textu na vysce y k dispozici na kazdou stranu od stredu.
//
// Na kulatem panelu se to muselo pocitat z tetivy kruhu; na obdelniku je to
// vzdy pulka sirky. Funkce zustava kvuli kodu prenasenemu z 0.6.1, ktery ji
// vola - a kdyby nekdy pribyla maska (zaobleny ram krabicky), ma to jedno
// misto, kde se to zmeni.
int  Layout_ChordHalf(int y);

// Kolik obdelniku je prave drzenych (diagnostika).
int  Layout_Count();

// Projde pevne pruhy a vypise pripadny prekryv na seriovou linku. Vola se
// jednou pri startu, kdyz je LAYOUT_DEBUG zapnute - odhali preklep v konstante
// driv, nez se z nej stanou dva popisky pres sebe.
void Layout_SelfTest();
