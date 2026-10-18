/*
 * Theme.h – barvy a rozložení obrazovky (320 x 480, na výšku)
 *
 * Barvy vychází z webu laskakit.cz:
 *   primární tyrkysová  #009BAA
 *   oranžová (akce)     #FF873C
 *   žlutá (hover)       #FAB400
 *   tmavá (text)        #2C2826
 *   světlá tyrkysová    #E6F5F7
 */
#pragma once
#include <Arduino.h>

// ====== Barvy (RGB565) ======
#define COL_BG          0x1082   // #141110  pozadí stránky (teplá černá)
#define COL_CARD        0x2103   // #26211F  karty / tlačítka
#define COL_CARD_HI     0x39A6   // #3A3431  stisknuté tlačítko, dráha slideru
#define COL_GRID        0x1945   // #1F2A2B  zhasnuté segmenty spektra

#define COL_TEAL        0x04D5   // #009BAA  LaskaKit primární
#define COL_TEAL_DIM    0x09C8   // #0E3B40
#define COL_TEAL_LIGHT  0xE7BE   // #E6F5F7
#define COL_ORANGE      0xFC27   // #FF873C  LaskaKit akční barva
#define COL_AMBER       0xFDA0   // #FAB400
#define COL_DARK        0x2944   // #2C2826  ikona na oranžovém tlačítku

#define COL_TEXT        0xFFFF
#define COL_MUTED       0x9C91   // #9A938F
#define COL_ERR         0xE249   // #E5484D

// ====== Displej ======
constexpr int SCR_W = 320;
constexpr int SCR_H = 480;
constexpr int MARGIN = 12;

// Hlavička
constexpr int HDR_H        = 44;               // pod ní 2px dvoubarevná linka
constexpr int HDR_ICONS_W  = 124;
constexpr int HDR_ICONS_X  = SCR_W - HDR_ICONS_W;
constexpr int HDR_SUN_CX   = 16;               // relativně k HDR_ICONS_X
constexpr int HDR_WIFI_CX  = 54;
constexpr int HDR_BAT_X    = 82;

// Karta se stanicí
constexpr int CARD_X = MARGIN;
constexpr int CARD_Y = 52;
constexpr int CARD_W = SCR_W - 2 * MARGIN;     // 296
constexpr int CARD_H = 164;
constexpr int CARD_PAD = 16;

constexpr int TITLE_X = CARD_X + CARD_PAD;     // běžící název skladby
constexpr int TITLE_Y = CARD_Y + 84;
constexpr int TITLE_W = CARD_W - 2 * CARD_PAD;
constexpr int TITLE_H = 24;

// Spektrum + VU
constexpr int SPEC_X = MARGIN;
constexpr int SPEC_Y = 226;
constexpr int SPEC_W = CARD_W;
constexpr int SPEC_H = 122;
constexpr int SPEC_BARS_H = 100;
constexpr int SPEC_SEGS   = 20;                // segmentů ve sloupci
constexpr int SPEC_GAP    = 4;                 // mezera mezi sloupci

// Hlasitost
constexpr int VOL_X = MARGIN;
constexpr int VOL_Y = 354;
constexpr int VOL_W = CARD_W;
constexpr int VOL_H = 40;
constexpr int VOL_TRACK_X0 = 36;               // relativně k VOL_X
constexpr int VOL_TRACK_X1 = 246;

// Ovládací tlačítka
constexpr int BTN_Y  = 400;
constexpr int BTN_H  = 80;
constexpr int BTN_CY = 40;                     // relativně k BTN_Y
constexpr int BTN_PREV_CX = 76;
constexpr int BTN_PLAY_CX = SCR_W / 2;
constexpr int BTN_NEXT_CX = SCR_W - 76;
constexpr int BTN_SIDE_R  = 27;
constexpr int BTN_PLAY_R  = 34;

// Seznam stanic
constexpr int LIST_Y      = 52;
constexpr int LIST_ROW_Y  = 100;
constexpr int LIST_ROW_H  = 46;
constexpr int LIST_ROWS   = 7;                 // řádků na stránku
constexpr int LIST_NAV_Y  = 426;
