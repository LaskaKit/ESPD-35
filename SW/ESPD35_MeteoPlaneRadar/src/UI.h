// =============================================================================
//  ESPD35_MeteoPlaneRadar - sdilene UI utility (barvy, globalni gfx, text, QR).
//  Port z petus/Meteo-PlaneRadar pro ESPD-3.5 (ILI9488 480x320).
// =============================================================================
#pragma once
#include <Arduino_GFX_Library.h>
#include "Config.h"

// Barvy (RGB565)
#define C_BLACK  0x0000
#define C_BLUE   0x001F
#define C_RED    0xF800
#define C_GREEN  0x07E0
#define C_WHITE  0xFFFF
#define C_YELLOW 0xFFE0
#define C_GRAY   0x8410
#define C_DKGRAY 0x2124
#define C_CYAN   0x05FF
#define C_ORANGE 0xFC00
#define C_PLBLUE 0x02DF   // modra pro letadla ve vysokych hladinach

// Globalni displej (off-screen canvas v PSRAM, definovany v .ino).
extern Arduino_GFX* gfx;

// Vycentrovany text pres celou sirku displeje (velikost 1-4).
void UI_TextCentered(const char* text, int cy, uint16_t color, uint8_t size);

// Vycentrovany text v obdelniku [x, x+w) - pouziva se pro texty nad mapou.
void UI_TextCenteredIn(const char* text, int x, int w, int cy,
                       uint16_t color, uint8_t size);

// Vykresli WiFi QR kod (pro pripojeni k AP). open=true -> sit bez hesla.
void UI_DrawWifiQR(const char* ssid, const char* password, bool open,
                   int x, int y, int size_px);
