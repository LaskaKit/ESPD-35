// =============================================================================
//  ESPD35_MeteoPlaneRadar - obrazovka radaru letadel (adsb.fi).
//  Port z petus/Meteo-PlaneRadar prizpusobeny obdelnikovemu displeji 480x320:
//    - MAPA vlevo (3/4), stred = poloha uzivatele
//    - DETAIL vpravo (1/4), automaticky NEJBLIZSI letadlo k poloze uzivatele
//
//  MERITKO (dulezite): projekce je izotropni - stejny pocet px na km ve
//  vodorovnem i svislem smeru. Diky tomu SEDI polohy letadel i podkres mest.
//  Stred mapy je (MAP_CX, MAP_CY), meritko scale = R_RADIUS / rozsah_km.
//  Protoze je mapove okno obdelnikove, definujeme rozsah jako svisly polomer;
//  vodorovne se diky sirsimu oknu ukaze o neco vetsi vyrez, ale meritko
//  zustava STEJNE v obou osach (zadne zkresleni vzdalenosti).
// =============================================================================
#include "ScreenPlanes.h"
#include "ADSB.h"
#include "Settings.h"
#include "CzBorder.h"
#include "UI.h"
#include "Config.h"

#include <WiFi.h>
#include <math.h>

// Dostupne rozsahy (polomer v km).
static const float RANGES_KM[] = {10.0f, 25.0f, 50.0f, 100.0f};
static const int   RANGE_COUNT = sizeof(RANGES_KM) / sizeof(RANGES_KM[0]);
static int s_rangeIdx = 1;

static float currentRange() { return RANGES_KM[s_rangeIdx]; }

static unsigned long s_nextFetch = 0;
static bool   s_dataOk = false;
static String s_status = "Start...";

// Zadost o spusteni WiFi/AP portalu (blokujici -> obsluha probiha v .ino).
static bool s_wantPortal = false;
bool ScreenPlanes_WantsPortal() { return s_wantPortal; }
void ScreenPlanes_ClearPortal() { s_wantPortal = false; }

// Geometrie tlacitek v pravem panelu.
static const int BTN_X       = MAP_W + 4;
static const int BTN_W       = PANEL_W - 8;
static const int BTN_H       = 28;
static const int BTN_UNITS_Y = 212;   // tlacitko: prepnuti jednotek
static const int BTN_WIFI_Y  = 246;   // tlacitko: WiFi + poloha (AP portal)

// Rucne vybrane letadlo (dotykem). Drzime pres ICAO hex - stabilni ID, ktere
// prezije prehazeni poradi v datech mezi stahovanimi. "" = automaticky nejblizsi.
static char s_selIcao[7] = "";
// Obrazovkove pozice + ICAO letadel z posledniho vykresleni (pro tap-to-select).
static int  s_planeX[ADSB_MAX];
static int  s_planeY[ADSB_MAX];
static char s_planeIcao[ADSB_MAX][7];
static int  s_planeN = 0;

// Dohleda index letadla v aktualnim poli podle ICAO. -1 = neni.
static int findIdxByIcao(const char* icao) {
  if (!icao || !icao[0]) return -1;
  const Aircraft* list = ADSB_List();
  int n = ADSB_Count();
  for (int i = 0; i < n; i++) if (strcmp(list[i].icao, icao) == 0) return i;
  return -1;
}

// -----------------------------------------------------------------------------
//  Projekce lat/lon -> obrazovka (sever nahoru), izotropni meritko.
//  clat/clon = poloha uzivatele (stred), rangeKm = zvoleny rozsah.
// -----------------------------------------------------------------------------
static void project(float lat, float lon, double clat, double clon,
                    float rangeKm, int* sx, int* sy) {
  float latr = clat * 0.0174532925f;
  float dxKm = (lon - clon) * 111.0f * cosf(latr);   // vychod (+)
  float dyKm = (lat - clat) * 111.0f;                // sever (+)
  float scale = (float)R_RADIUS / rangeKm;           // px na km (STEJNE X i Y)
  *sx = MAP_CX + (int)lroundf(dxKm * scale);
  *sy = MAP_CY - (int)lroundf(dyKm * scale);
}

// Vzdalenost letadla od stredu (uzivatele) v km - pro vyber nejblizsiho.
static float distKm(float lat, float lon, double clat, double clon) {
  float latr = clat * 0.0174532925f;
  float dxKm = (lon - clon) * 111.0f * cosf(latr);
  float dyKm = (lat - clat) * 111.0f;
  return sqrtf(dxKm * dxKm + dyKm * dyKm);
}

// Wrapper pro CzBorder (mesta/obrys) - stejna projekce jako letadla.
static void cityProject(float lat, float lon, int* sx, int* sy) {
  project(lat, lon, Settings_Lat(), Settings_Lon(), currentRange(), sx, sy);
}

// Barva letadla podle nadmorske vysky (vzdy ve stopach, nezavisle na jednotkach).
static uint16_t altColor(float altFt) {
  if (altFt <      1) return C_GRAY;     // vyska neznama
  if (altFt <   3000) return C_RED;      // priblizeni, vrtulniky
  if (altFt <  10000) return C_ORANGE;   // stoupani / klesani
  if (altFt <  20000) return C_YELLOW;   // stredni hladiny
  if (altFt <  30000) return C_GREEN;    // nizsi cestovni
  return C_PLBLUE;                        // cestovni hladina, prelety
}

// Ikona letadla natocena podle kurzu. Bez kurzu -> krouzek s teckou.
static void drawPlane(int x, int y, float trackDeg, bool hasTrack, uint16_t col) {
  if (!hasTrack) {
    gfx->drawCircle(x, y, 7, col);
    gfx->fillCircle(x, y, 2, col);
    return;
  }
  float a = trackDeg * 0.0174532925f;
  float ca = cosf(a), sa = sinf(a);
  auto rot = [&](float right, float fwd, int* ox, int* oy) {
    *ox = x + (int)(right * ca + fwd * sa);
    *oy = y + (int)(right * sa - fwd * ca);
  };
  const float P[10][2] = {
    { 0,  12}, { 3,  1}, { 13, -8}, { 3, -5}, { 3, -7},
    { 0, -12}, {-3, -7}, {-3, -5}, {-13, -8}, {-3,  1}
  };
  int px[10], py[10];
  for (int i = 0; i < 10; i++) rot(P[i][0], P[i][1], &px[i], &py[i]);
  for (int i = 0; i < 10; i++) {
    int j = (i + 1) % 10;
    gfx->fillTriangle(x, y, px[i], py[i], px[j], py[j], col);
  }
}

void ScreenPlanes_Enter() {
  s_rangeIdx = Settings_RangeIdx();
  if (s_rangeIdx >= RANGE_COUNT) s_rangeIdx = 1;
  s_nextFetch = 0;
}

bool ScreenPlanes_Tick() {
  if (WiFi.status() != WL_CONNECTED) { s_status = "Ceka na WiFi"; return false; }
  if (millis() >= s_nextFetch) {
    s_status = "Stahuji...";
    s_dataOk = ADSB_Fetch(Settings_Lat(), Settings_Lon(), currentRange());
    s_status = s_dataOk ? "OK" : "Chyba";
    s_nextFetch = millis() + (s_dataOk ? 5000 : 15000);
    return true;
  }
  return false;
}

void ScreenPlanes_ChangeRange(int dir) {
  s_rangeIdx = (s_rangeIdx + dir + RANGE_COUNT) % RANGE_COUNT;
  Settings_SetRangeIdx(s_rangeIdx);
  s_nextFetch = 0;   // hned stahnout pro novy rozsah
}

void ScreenPlanes_NextRange() { ScreenPlanes_ChangeRange(+1); }

void ScreenPlanes_ToggleUnits() {
  Settings_SetMetricUnits(!Settings_MetricUnits());
}

// Kratky dotyk (tap). Vraci true, kdyz je potreba prekreslit.
bool ScreenPlanes_HandleTap(int x, int y) {
  // Dotek v pravem panelu -> tlacitka (jednotky / WiFi+poloha).
  if (x >= MAP_W) {
    if (x >= BTN_X && x <= BTN_X + BTN_W &&
        y >= BTN_UNITS_Y && y <= BTN_UNITS_Y + BTN_H) {
      ScreenPlanes_ToggleUnits();
      return true;
    }
    if (x >= BTN_X && x <= BTN_X + BTN_W &&
        y >= BTN_WIFI_Y && y <= BTN_WIFI_Y + BTN_H) {
      s_wantPortal = true;     // spusti AP portal (obsluha v .ino)
      return true;
    }
    return false;              // jinam v panelu -> nic
  }
  // Dotek v mape -> najdi nejblizsi vykreslene letadlo (do ~26 px).
  int best = -1;
  long bestD = 26L * 26L;
  for (int i = 0; i < s_planeN; i++) {
    long dx = s_planeX[i] - x, dy = s_planeY[i] - y;
    long d = dx * dx + dy * dy;
    if (d < bestD) { bestD = d; best = i; }
  }
  if (best >= 0) {
    strncpy(s_selIcao, s_planeIcao[best], sizeof(s_selIcao) - 1);
    s_selIcao[sizeof(s_selIcao) - 1] = '\0';
  } else {
    s_selIcao[0] = '\0';   // prazdny tap -> zpet na automaticky nejblizsi
  }
  return true;
}

// -----------------------------------------------------------------------------
//  Detailovy panel vpravo (1/4) - nejblizsi letadlo k poloze uzivatele.
// -----------------------------------------------------------------------------
// Tlacitko v panelu: vyplneny obdelnik s vycentrovanym popiskem.
static void drawButton(int y, uint16_t bg, const char* label) {
  gfx->fillRoundRect(BTN_X, y, BTN_W, BTN_H, 6, bg);
  gfx->drawRoundRect(BTN_X, y, BTN_W, BTN_H, 6, C_WHITE);
  gfx->setTextSize(1); gfx->setTextColor(C_BLACK);
  int tw = strlen(label) * 6;
  int lx = BTN_X + (BTN_W - tw) / 2; if (lx < BTN_X + 2) lx = BTN_X + 2;
  gfx->setCursor(lx, y + (BTN_H - 8) / 2);
  gfx->print(label);
}

static void drawPanel(int detailIdx, float detailDistKm, bool pinned) {
  const int x0 = MAP_W;
  gfx->fillRect(x0, 0, PANEL_W, LCD_HEIGHT, C_DKGRAY);     // pozadi panelu
  gfx->drawFastVLine(x0, 0, LCD_HEIGHT, C_CYAN);           // oddelovaci cara

  const int tx = x0 + 8;
  const bool metric = Settings_MetricUnits();

  gfx->setTextSize(1); gfx->setTextColor(C_CYAN);
  gfx->setCursor(tx, 8);
  gfx->print(pinned ? "VYBRANE" : "NEJBLIZSI");   // kratky nadpis

  if (detailIdx < 0) {
    gfx->setTextSize(2); gfx->setTextColor(C_GRAY);
    gfx->setCursor(tx, 44); gfx->print("zadne");
    gfx->setCursor(tx, 66); gfx->print("v okruhu");
  } else {
    const Aircraft& ac = ADSB_List()[detailIdx];
    char line[40];

    // Volacka (nadpis).
    gfx->setTextSize(2); gfx->setTextColor(C_YELLOW);
    gfx->setCursor(tx, 30);
    gfx->print(ac.callsign[0] ? ac.callsign : "?");

    int ty = 62;
    gfx->setTextSize(1); gfx->setTextColor(C_WHITE);

    if (ac.type[0]) snprintf(line, sizeof(line), "Typ:  %s", ac.type);
    else            snprintf(line, sizeof(line), "Typ:  -");
    gfx->setCursor(tx, ty); gfx->print(line); ty += 22;

    snprintf(line, sizeof(line), "Vzdal: %.1f km", detailDistKm);
    gfx->setCursor(tx, ty); gfx->print(line); ty += 22;

    if (metric) snprintf(line, sizeof(line), "Vyska: %.0f m", ac.altFt * 0.3048f);
    else        snprintf(line, sizeof(line), "Vyska: %.0f ft", ac.altFt);
    gfx->setCursor(tx, ty); gfx->print(line); ty += 22;

    if (metric) snprintf(line, sizeof(line), "Rychl: %.0f km/h", ac.gsKt * 1.852f);
    else        snprintf(line, sizeof(line), "Rychl: %.0f kt", ac.gsKt);
    gfx->setCursor(tx, ty); gfx->print(line); ty += 22;

    if (ac.hasTrack) snprintf(line, sizeof(line), "Kurz:  %.0f deg", ac.track);
    else             snprintf(line, sizeof(line), "Kurz:  neznamy");
    gfx->setCursor(tx, ty); gfx->print(line); ty += 22;

    // Stoupani/klesani vc. kratke sipky (aby text nepretekal panel).
    const char* ar = ac.baroRate > 100 ? "^" : (ac.baroRate < -100 ? "v" : "-");
    if (metric) snprintf(line, sizeof(line), "V/S: %.1f m/s %s", ac.baroRate * 0.00508f, ar);
    else        snprintf(line, sizeof(line), "V/S: %.0f ft/m %s", ac.baroRate, ar);
    gfx->setCursor(tx, ty); gfx->print(line);
  }

  // Tlacitka dole: prepnuti jednotek + spusteni WiFi/AP portalu.
  drawButton(BTN_UNITS_Y, C_CYAN,   metric ? "Jednotky: metricke" : "Jednotky: letecke");
  drawButton(BTN_WIFI_Y,  C_YELLOW, "WiFi + poloha (AP)");

  gfx->setTextSize(1); gfx->setTextColor(C_GRAY);
  gfx->setCursor(tx, LCD_HEIGHT - 12); gfx->print("laskakit.cz");
}

// Legenda barev letadel podle letove hladiny (vlevo na mape).
static void drawLegend() {
  static const struct { uint16_t col; const char* lbl; } L[] = {
    { C_PLBLUE, ">30"   },
    { C_GREEN,  "20-30" },
    { C_YELLOW, "10-20" },
    { C_ORANGE, "3-10"  },
    { C_RED,    "<3"    },
    { C_GRAY,   "?"     },
  };
  const int lx = 4, ly = 20;
  gfx->setTextSize(1); gfx->setTextColor(C_GRAY);
  gfx->setCursor(lx, ly); gfx->print("FL x1000ft");
  for (int i = 0; i < 6; i++) {
    int ry = ly + 12 + i * 13;
    gfx->fillRect(lx, ry, 9, 9, L[i].col);
    gfx->drawRect(lx, ry, 9, 9, C_DKGRAY);
    gfx->setTextColor(C_WHITE);
    gfx->setCursor(lx + 13, ry + 1);
    gfx->print(L[i].lbl);
  }
}

// Zony, kam letadla nekreslime (prekryvala by legendu a rozsah vlevo).
static bool inReservedZone(int sx, int sy) {
  if (sx < 70 && sy < 112) return true;   // pocet letadel + legenda (vlevo nahore)
  if (sx < 58 && sy > 286) return true;   // rozsah + indikator (vlevo dole)
  return false;
}

void ScreenPlanes_Draw() {
  gfx->fillScreen(C_BLACK);
  float range = currentRange();
  const double clat = Settings_Lat(), clon = Settings_Lon();

  // --- Podkres: obrys CR + mesta (stejna projekce jako letadla) ---
  CzBorder_Draw(cityProject, C_GRAY);
  {
    bool showFull  = (range <= 25.0f);
    bool showSmall = (range <= 50.0f);
    // Polomer tak, aby mesta zustala v mapovem okne a nezasahla do panelu.
    int cityRad = MAP_H / 2 - 2;
    CzBorder_DrawCities(cityProject, MAP_CX, MAP_CY, cityRad,
                        C_DKGRAY, C_GRAY, showFull, showSmall);
  }

  // --- Kruhy rozsahu + zamerny kriz na poloze uzivatele ---
  gfx->drawCircle(MAP_CX, MAP_CY, R_RADIUS, C_DKGRAY);
  gfx->drawCircle(MAP_CX, MAP_CY, R_RADIUS / 2, C_DKGRAY);
  gfx->drawFastHLine(MAP_CX - 8, MAP_CY, 16, C_WHITE);
  gfx->drawFastVLine(MAP_CX, MAP_CY - 8, 16, C_WHITE);

  // --- Nejblizsi letadlo (fallback pro detail, kdyz nic neni vybrano) ---
  int nearestIdx = -1;
  float nearestDist = 1e9f;
  const Aircraft* list = ADSB_List();
  int n = ADSB_Count();
  for (int i = 0; i < n; i++) {
    if (list[i].onGround) continue;
    if (list[i].lat == 0 && list[i].lon == 0) continue;
    float d = distKm(list[i].lat, list[i].lon, clat, clon);
    if (d < nearestDist) { nearestDist = d; nearestIdx = i; }
  }

  // Letadlo do detailu: rucne vybrane (dle ICAO), jinak automaticky nejblizsi.
  int selIdx = findIdxByIcao(s_selIcao);
  if (s_selIcao[0] && selIdx < 0) s_selIcao[0] = '\0';   // vybrane zmizelo -> auto
  int detailIdx = (selIdx >= 0) ? selIdx : nearestIdx;

  // --- Vykresleni letadel (jen v mapovem okne) ---
  int shown = 0;
  s_planeN = 0;                        // znovu naplnime pozice pro tap-to-select
  for (int i = 0; i < n; i++) {
    if (list[i].onGround) continue;
    if (list[i].lat == 0 && list[i].lon == 0) continue;
    int sx, sy;
    project(list[i].lat, list[i].lon, clat, clon, range, &sx, &sy);
    if (sx < 2 || sx >= MAP_W - 2 || sy < 2 || sy >= MAP_H - 2) continue;  // mimo mapu
    if (inReservedZone(sx, sy)) continue;   // pod legendou/rozsahem letadla nekreslime

    // Ulozit pozici + ICAO pro dotykovy vyber.
    if (s_planeN < ADSB_MAX) {
      s_planeX[s_planeN] = sx; s_planeY[s_planeN] = sy;
      strncpy(s_planeIcao[s_planeN], list[i].icao, 6);
      s_planeIcao[s_planeN][6] = '\0';
      s_planeN++;
    }

    if (i == detailIdx) {   // zvyrazneni letadla v detailu bilym krouzkem
      gfx->drawCircle(sx, sy, 15, C_WHITE);
      gfx->drawCircle(sx, sy, 17, C_WHITE);
    }
    drawPlane(sx, sy, list[i].track, list[i].hasTrack, altColor(list[i].altFt));
    if (list[i].callsign[0]) {
      int len = strlen(list[i].callsign);
      int tw = len * 6;               // font size 1
      int lx = sx - tw / 2;
      if (lx < 1) lx = 1;
      if (lx + tw > MAP_W - 2) lx = MAP_W - 2 - tw;
      gfx->setTextSize(1); gfx->setTextColor(C_WHITE);
      gfx->setCursor(lx, sy + 16);
      gfx->print(list[i].callsign);
    }
    shown++;
  }

  // --- Pocet letadel / stav: maly font, vlevo nahore ---
  gfx->setTextSize(1);
  if (WiFi.status() != WL_CONNECTED || !s_dataOk) {
    gfx->setTextColor(C_YELLOW);
    gfx->setCursor(4, 4); gfx->print(s_status);
  } else {
    char sub[24];
    snprintf(sub, sizeof(sub), "%d letadel", shown);
    gfx->setTextColor(C_CYAN);
    gfx->setCursor(4, 4); gfx->print(sub);
  }

  // --- Legenda barev / letovych hladin (vlevo) ---
  drawLegend();

  // --- Rozsah + indikator rozsahu: maly font, vlevo dole ---
  char rbuf[16];
  snprintf(rbuf, sizeof(rbuf), "%.0f km", range);
  gfx->setTextSize(1); gfx->setTextColor(C_YELLOW);
  gfx->setCursor(4, LCD_HEIGHT - 22); gfx->print(rbuf);
  int dotGap = 14, dotY = LCD_HEIGHT - 8, startX = 8;
  for (int i = 0; i < RANGE_COUNT; i++) {
    int x = startX + i * dotGap;
    if (i == s_rangeIdx) gfx->fillCircle(x, dotY, 3, C_YELLOW);
    else                 gfx->drawCircle(x, dotY, 3, C_GRAY);
  }

  // --- Detailovy panel vpravo (prekryje pripadny presah podkresu) ---
  float detailDist = (detailIdx >= 0)
      ? distKm(list[detailIdx].lat, list[detailIdx].lon, clat, clon) : 0.0f;
  drawPanel(detailIdx, detailDist, selIdx >= 0);
}
