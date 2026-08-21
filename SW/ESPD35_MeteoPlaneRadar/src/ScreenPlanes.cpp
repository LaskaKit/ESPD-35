// =============================================================================
//  ESPD35_MeteoPlaneRadar - obrazovka radaru letadel (adsb.fi).
//  Layout pro obdelnikovy displej 480x320:
//    - MAPA vlevo (3/4), stred = poloha uzivatele
//    - DETAIL vpravo (1/4)
//
//  MERITKO (dulezite): projekce je izotropni - stejny pocet px na km ve
//  vodorovnem i svislem smeru. Diky tomu SEDI polohy letadel i podkres mest.
//  Stred mapy je (MAP_CX, MAP_CY), meritko scale = R_RADIUS / rozsah_km.
//  Protoze je mapove okno obdelnikove, definujeme rozsah jako svisly polomer;
//  vodorovne se diky sirsimu oknu ukaze o neco vetsi vyrez, ale meritko
//  zustava STEJNE v obou osach (zadne zkresleni vzdalenosti).
//
//  ORIENTACE MAPY: uzivatel v Nastaveni voli, ktery svetovy smer je NAHORE
//  (smer, kterym se diva z okna). Otaci se PROJEKCE, ne displej - obrys, mesta
//  i ikony letadel se tak otoci spolecne a dotykove souradnice zustavaji platne.
// =============================================================================
#include "ScreenPlanes.h"
#include "Layout.h"
#include "Route.h"
#include "Outside.h"
#include "Lang.h"
#include "ADSB.h"
#include "Settings.h"
#include "EuBorder.h"
#include "UI.h"
#include "Config.h"

#include <WiFi.h>
#include <math.h>
#include <string.h>
#include <time.h>

// Dostupne rozsahy (polomer v km) - z Config.h.
static const float RANGES_KM[] = PLANE_RANGES_KM;
static const int   RANGE_COUNT = sizeof(RANGES_KM) / sizeof(RANGES_KM[0]);
static int s_rangeIdx = 1;

static float currentRange() { return RANGES_KM[s_rangeIdx]; }

// Interval stahovani podle rozsahu. Vetsi okruh vraci vic dat a je mene casove
// kriticky, takze se stahuje redceji - setrnejsi k bezplatnemu API adsb.fi.
static unsigned long basePeriodMs() {
  float r = currentRange();
  if (r <= ADSB_NEAR_KM) return ADSB_PERIOD_NEAR_MS;
  if (r <= ADSB_MID_KM)  return ADSB_PERIOD_MID_MS;
  return ADSB_PERIOD_FAR_MS;
}

static unsigned long s_nextFetch = 0;
static bool   s_dataOk = false;
static String s_status = "Start...";

// -----------------------------------------------------------------------------
//  Rucne vybrane letadlo
//
//  Drzi se pres ICAO hex - stabilni ID, ktere prezije prehazeni poradi v datech
//  mezi stazenimi. Index v poli se drzet NESMI: seznam se pri kazdem stazeni
//  stavi znovu a jedno letadlo, ktere opusti oblast, posune vsechna za nim.
//  "" = nic vybrano -> detail ukazuje automaticky nejblizsi letadlo.
// -----------------------------------------------------------------------------
static char s_selHex[8] = "";

// Tolerance vypadku. adsb.fi obcas letadlo v jednom stazeni vynecha a v dalsim
// ho zase posle. Kdyby se vyber zrusil hned pri prvnim vypadku, vypadalo by to,
// ze detail sam uskocil na jine letadlo. Behem grace periody se drzi posledni
// zname hodnoty (s_selCache) s poznamkou "signal ztracen".
static int      s_selMiss   = 0;    // po sobe jdouci stazeni bez tohohle letadla
static Aircraft s_selCache;         // posledni znama data vybraneho letadla
static bool     s_selCacheOk = false;

// Obrazovkove pozice + ICAO letadel z posledniho vykresleni (pro tap-to-select).
static int  s_planeX[ADSB_MAX];
static int  s_planeY[ADSB_MAX];
static char s_planeHex[ADSB_MAX][8];
static int  s_planeN = 0;

bool ScreenPlanes_DetailOpen() { return s_selHex[0] != '\0'; }

// reason = kratky text do seriove linky, aby slo poznat PROC se vyber zrusil
// (falesny dotyk vs. letadlo skutecne zmizelo z dat).
static void selectNone(const char* reason) {
#if TOUCH_DEBUG
  if (s_selHex[0]) Serial.printf("SEL: zruseno (%s) hex=%s\n", reason, s_selHex);
#else
  (void)reason;
#endif
  s_selHex[0] = '\0';
  s_selMiss = 0;
  s_selCacheOk = false;
  Route_Clear();          // zrusit i pripadny cekajici dotaz na trasu
}

static void selectHex(const char* hex) {
  strncpy(s_selHex, hex, sizeof(s_selHex) - 1);
  s_selHex[sizeof(s_selHex) - 1] = '\0';
  s_selMiss = 0;
  s_selCacheOk = false;
#if TOUCH_DEBUG
  Serial.printf("SEL: vybrano hex=%s\n", s_selHex);
#endif
}

void ScreenPlanes_CloseDetail() { selectNone("rucne"); }

// -----------------------------------------------------------------------------
//  Otoceni mapy - viz komentar v hlavicce souboru.
//
//      uhel na obrazovce pro azimut b  =  b - topBearing   (0 = nahoru, po smeru)
//
//  Pri pohledu na vychod (top = 90) je sever vlevo, presne jako ve skutecnosti.
//  Kdyby se zadavalo "o kolik mapu otocit", byla by potreba opacna hodnota
//  (360 - azimut) a mapa by pusobila zrcadlove.
// -----------------------------------------------------------------------------
static float    s_rotSin = 0.0f, s_rotCos = 1.0f;
static uint16_t s_topDeg = 0xFFFF;   // vynuti prvni prepocet

static void refreshRotation() {
  uint16_t deg = Settings_TopBearing();
  if (deg == s_topDeg) return;
  s_topDeg = deg;
  float r = (float)deg * 0.0174532925f;
  s_rotSin = sinf(r);
  s_rotCos = cosf(r);
}

// -----------------------------------------------------------------------------
//  Projekce lat/lon -> obrazovka, izotropni meritko, s otocenim.
//  clat/clon = poloha uzivatele (stred), rangeKm = zvoleny rozsah.
// -----------------------------------------------------------------------------
static void project(float lat, float lon, double clat, double clon,
                    float rangeKm, int* sx, int* sy) {
  float latr = clat * 0.0174532925f;
  float dxKm = (lon - clon) * 111.0f * cosf(latr);   // vychod (+)
  float dyKm = (lat - clat) * 111.0f;                // sever (+)
  // Otoc lokalni vektor vychod/sever tak, aby zvoleny azimut skoncil nahore.
  float rx = dxKm * s_rotCos - dyKm * s_rotSin;
  float ry = dxKm * s_rotSin + dyKm * s_rotCos;
  float scale = (float)R_RADIUS / rangeKm;           // px na km (STEJNE X i Y)
  *sx = MAP_CX + (int)lroundf(rx * scale);
  *sy = MAP_CY - (int)lroundf(ry * scale);
}

// Vzdalenost letadla od stredu (uzivatele) v km - pro vyber nejblizsiho.
// Na otoceni mapy nezavisi.
static float distKm(float lat, float lon, double clat, double clon) {
  float latr = clat * 0.0174532925f;
  float dxKm = (lon - clon) * 111.0f * cosf(latr);
  float dyKm = (lat - clat) * 111.0f;
  return sqrtf(dxKm * dxKm + dyKm * dyKm);
}

// Wrapper pro EuBorder (hranice a mesta) - stejna projekce jako letadla.
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
// trackDeg uz musi byt OPRAVENY o otoceni mapy (viz volani v Draw).
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
  s_rangeIdx = Settings_PlaneRange();
  if (s_rangeIdx >= RANGE_COUNT) s_rangeIdx = 1;   // ochrana proti stare hodnote
  s_nextFetch = 0;
  refreshRotation();
}

bool ScreenPlanes_Tick() {
  if (WiFi.status() != WL_CONNECTED) { s_status = "Ceka na WiFi"; return false; }
  if (millis() >= s_nextFetch) {
    s_status = "Stahuji...";
    s_dataOk = ADSB_Fetch(Settings_Lat(), Settings_Lon(), currentRange());
    s_status = s_dataOk ? "OK" : "Chyba";

    // Vyhodnoceni vyberu se dela TADY, tedy jednou za stazeni. V Draw() by se
    // pocitalo vicekrat za sekundu a grace perioda by vyprsela behem chvile.
    if (s_dataOk && s_selHex[0]) {
      int idx = ADSB_FindByHex(s_selHex);
      if (idx >= 0) {
        s_selCache   = ADSB_List()[idx];
        s_selCacheOk = true;
        s_selMiss    = 0;

        // Vybrane letadlo OPUSTILO ZOBRAZENOU MAPU. Grace perioda je urcena
        // na vypadky dat, ne na tohle - drzet v panelu stroj, ktery uz na
        // obrazovce neni, nedava smysl. Vyber se proto zrusi hned a panel se
        // vrati k automaticky nejblizsimu letadlu.
        if (distKm(s_selCache.lat, s_selCache.lon, Settings_Lat(), Settings_Lon())
            > currentRange()) {
          selectNone("letadlo opustilo mapu");
        }
      } else if (++s_selMiss > DETAIL_GRACE_POLLS) {
        // Vypadlo z dat i po tolerovanych vypadcich - zpet na nejblizsi.
        selectNone("letadlo zmizelo z dat");
      }
    }

    // Normalni kadence podle rozsahu; po chybe dvojnasobek intervalu.
    unsigned long period = basePeriodMs();
    s_nextFetch = millis() + (s_dataOk ? period : period * 2);
    return true;
  }
  return false;
}

void ScreenPlanes_ChangeRange(int dir) {
  s_rangeIdx = (s_rangeIdx + dir + RANGE_COUNT) % RANGE_COUNT;
  Settings_SetPlaneRange(s_rangeIdx);   // zapis do NVS je odlozeny
  s_nextFetch = 0;                      // hned stahnout pro novy rozsah
}

void ScreenPlanes_NextRange() { ScreenPlanes_ChangeRange(+1); }

void ScreenPlanes_ToggleUnits() {
  Settings_SetMetricUnits(!Settings_MetricUnits());
}

// Kratky dotyk (tap). Vraci true, kdyz je potreba prekreslit.
bool ScreenPlanes_HandleTap(int x, int y) {
  // Dotek v pravem panelu -> zrusit rucni vyber (zpet na nejblizsi).
  // V panelu uz nejsou zadna tlacitka, takze se neni s cim trefit vedle.
  if (x >= MAP_W) {
    if (!s_selHex[0]) return false;
    selectNone("tap do panelu");
    return true;
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
    selectHex(s_planeHex[best]);
  } else {
    if (!s_selHex[0]) return false;
    selectNone("tap mimo letadlo");
  }
  return true;
}

// -----------------------------------------------------------------------------
//  Detailovy panel vpravo (1/4).
//
//  ac == nullptr    -> v okruhu neni zadne letadlo
//  pinned           -> letadlo je rucne zafixovane (jinak automaticky nejblizsi)
//  stale            -> data jsou z cache, letadlo v poslednim stazeni chybelo
// -----------------------------------------------------------------------------
// Rozvrzeni radku v detailovem panelu. Sest radku po PANEL_ROW_H pixelech
// zacina na PANEL_ROW_Y0 a konci na 58 + 6*29 = 232, tedy nad oddelovacem
// trasy na y = 236. Kdyz se tyhle konstanty zmeni, musi to porad platit.
#define PANEL_ROW_Y0  58
#define PANEL_ROW_H   29
#define PANEL_SEP_Y  236

static void drawPanel(const Aircraft* ac, float distKmVal, bool pinned, bool stale) {
  const int x0 = MAP_W;
  gfx->fillRect(x0, 0, PANEL_W, LCD_HEIGHT, C_DKGRAY);     // pozadi panelu
  gfx->drawFastVLine(x0, 0, LCD_HEIGHT, C_CYAN);           // oddelovaci cara

  const int tx = x0 + 6;
  const bool metric = Settings_MetricUnits();

  gfx->setTextSize(1); gfx->setTextColor(C_CYAN);
  gfx->setCursor(tx, 6);
  gfx->print(pinned ? "VYBRANE" : "NEJBLIZSI");

  // Nouzovy kod odpovidace prebije nadpis - je to jedina informace na teto
  // obrazovce, kvuli ktere stoji za to prerusit, co uzivatel prave dela.
  if (ac && Settings_SquawkAlert()) {
    const char* em = ADSB_EmergencyCode(*ac);
    if (em) {
      const char* word = (strcmp(em, SQUAWK_HIJACK) == 0) ? T(S_HIJACK)
                       : (strcmp(em, SQUAWK_RADIO)  == 0) ? T(S_RADIO_FAIL)
                                                          : T(S_EMERGENCY);
      gfx->fillRect(x0 + 1, 2, PANEL_W - 2, 14, C_RED);
      gfx->setTextColor(C_WHITE);
      gfx->setCursor(tx, 6);
      gfx->print(word);
    }
  }

  if (!ac) {
    gfx->setTextSize(2); gfx->setTextColor(C_GRAY);
    gfx->setCursor(tx, 40); gfx->print(T(S_NONE_IN_RANGE));
    gfx->setCursor(tx, 62); gfx->print(T(S_IN_RANGE));
  } else {
    char line[40];

    // Volacka (nadpis). Pri size 2 se do panelu vejde 9 znaku.
    gfx->setTextSize(2); gfx->setTextColor(C_YELLOW);
    gfx->setCursor(tx, 18);
    gfx->print(ac->callsign[0] ? ac->callsign : "?");

    // ICAO hex - podle nej se letadlo drzi mezi stazenimi.
    gfx->setTextSize(1); gfx->setTextColor(C_GRAY);
    gfx->setCursor(tx, 38); gfx->print(ac->hex);

    // Upozorneni, ze data uz nejsou cerstva (grace perioda).
    if (stale) {
      gfx->setTextColor(C_YELLOW);
      gfx->setCursor(tx, 52); gfx->print(T(S_SIGNAL_LOST));
    }

    // --- Radky detailu: maly popisek, VELKA hodnota, mala jednotka --------
    //
    // Panel je siroky PANEL_W (124 px), takze pri velikosti 1 se vejde 19
    // znaku a pri velikosti 2 devet. Driv byl cely radek velikosti 1 vcetne
    // hodnoty ("Vyska: 35000 ft") - hodnota se ztracela mezi popiskem
    // a jednotkou. Nove nese velikost 2 jen to cislo, ktere clovek hleda;
    // popisek a jednotka zustavaji male.
    //
    // Sirka se HLIDA: kdyby se hodnota s jednotkou do panelu nevesla, spadne
    // zpatky na velikost 1, misto aby pretekla do mapy.
    int ty = PANEL_ROW_Y0;
    for (int row = 0; row < 6; row++) {
      char label[12] = "", value[12] = "", unit[8] = "";
      switch (row) {
        case 0:
          snprintf(label, sizeof(label), "Typ");
          snprintf(value, sizeof(value), "%s", ac->type[0] ? ac->type : "-");
          break;
        case 1:
          snprintf(label, sizeof(label), "Vzdalenost");
          snprintf(value, sizeof(value), "%.1f", distKmVal);
          snprintf(unit, sizeof(unit), "km");
          break;
        case 2:
          snprintf(label, sizeof(label), "Vyska");
          if (metric) { snprintf(value, sizeof(value), "%.0f", ac->altFt * 0.3048f);
                        snprintf(unit, sizeof(unit), "m"); }
          else        { snprintf(value, sizeof(value), "%.0f", ac->altFt);
                        snprintf(unit, sizeof(unit), "ft"); }
          break;
        case 3:
          snprintf(label, sizeof(label), "Rychlost");
          if (metric) { snprintf(value, sizeof(value), "%.0f", ac->gsKt * 1.852f);
                        snprintf(unit, sizeof(unit), "km/h"); }
          else        { snprintf(value, sizeof(value), "%.0f", ac->gsKt);
                        snprintf(unit, sizeof(unit), "kt"); }
          break;
        case 4:
          snprintf(label, sizeof(label), "Kurz");
          if (ac->hasTrack) { snprintf(value, sizeof(value), "%.0f", ac->track);
                              snprintf(unit, sizeof(unit), "deg"); }
          else                snprintf(value, sizeof(value), "?");
          break;
        default:
          snprintf(label, sizeof(label), "Stoupani");
          if (metric) { snprintf(value, sizeof(value), "%.1f", ac->baroRate * 0.00508f);
                        snprintf(unit, sizeof(unit), "m/s"); }
          else        { snprintf(value, sizeof(value), "%.0f", ac->baroRate);
                        snprintf(unit, sizeof(unit), "ft/m"); }
          break;
      }

      // Popisek
      gfx->setTextSize(1); gfx->setTextColor(C_GRAY);
      gfx->setCursor(tx, ty); gfx->print(label);

      // Hodnota - velikost 2, kdyz se vejde i s jednotkou, jinak velikost 1.
      const int avail = PANEL_W - 10;
      int vSize = 2;
      int vw = (int)strlen(value) * 6 * vSize;
      const int uw = (int)strlen(unit) * 6;
      if (vw + (unit[0] ? uw + 3 : 0) > avail) { vSize = 1; vw = (int)strlen(value) * 6; }

      uint16_t vCol = C_WHITE;
      if (row == 5) vCol = (ac->baroRate > 100) ? C_GREEN
                         : (ac->baroRate < -100) ? C_ORANGE : C_GRAY;

      gfx->setTextSize((uint8_t)vSize); gfx->setTextColor(vCol);
      gfx->setCursor(tx, ty + 10); gfx->print(value);

      if (unit[0]) {
        gfx->setTextSize(1); gfx->setTextColor(C_GRAY);
        // Jednotka sedi na spodni hrane cisla, ne na horni - jinak by
        // "vlala" nad radkem.
        gfx->setCursor(tx + vw + 3, ty + 10 + (vSize == 2 ? 8 : 0));
        gfx->print(unit);
      }

      // Sipka stoupani/klesani za jednotku.
      if (row == 5) {
        const char* ar = ac->baroRate > 100 ? "^" : (ac->baroRate < -100 ? "v" : "-");
        gfx->setTextSize(1); gfx->setTextColor(vCol);
        gfx->setCursor(tx + vw + 3 + uw + 4, ty + 10 + (vSize == 2 ? 8 : 0));
        gfx->print(ar);
      }

      ty += PANEL_ROW_H;
    }
  }

  // --- Odkud a kam leti (adsbdb.com) ---
  //
  // Pta se jen na VYBRANE letadlo, ne na cely seznam, a odpoved se kesuje.
  // Spousta letu zadnou trasu v databazi nema (vseobecne letectvi, vojaci,
  // vrtulniky) - to je normalni vysledek, ne chyba, a proste se nic neukaze.
  //
  // Do panelu sirokeho 124 px se vejde 20 znaku pri velikosti 1, takze se
  // kresli kody letist (PRG, LHR), ne cele nazvy mest.
  gfx->drawFastHLine(x0 + 4, PANEL_SEP_Y, PANEL_W - 8, C_GRAY);
  {
    int ry = PANEL_SEP_Y + 8;
    const RouteState rs = Route_GetState();
    if (rs == ROUTE_WAIT) {
      gfx->setTextSize(1); gfx->setTextColor(C_DKGRAY);
      gfx->setCursor(tx, ry); gfx->print("zjistuji trasu...");
      ry += 14;
    } else if (rs == ROUTE_OK) {
      const RouteInfo* rt = Route_Get();
      char line[32];
      gfx->setTextSize(1);
      if (rt->from[0]) {
        gfx->setTextColor(C_WHITE);
        snprintf(line, sizeof(line), "Z:  %s", rt->from);
        gfx->setCursor(tx, ry); gfx->print(line); ry += 13;
      }
      if (rt->to[0]) {
        gfx->setTextColor(C_WHITE);
        snprintf(line, sizeof(line), "Do: %s", rt->to);
        gfx->setCursor(tx, ry); gfx->print(line); ry += 13;
      }
      if (rt->reg[0]) {
        gfx->setTextColor(C_GRAY);
        gfx->setCursor(tx, ry); gfx->print(rt->reg); ry += 13;
      }
    }

    // Napoveda k ovladani se kresli az pod trasou a jen kdyz na ni zbylo misto -
    // trasa je informace, napoveda jen pripominka.
    if (ry <= 268) {
      gfx->setTextSize(1); gfx->setTextColor(C_DKGRAY);
      gfx->setCursor(tx, ry + 4);  gfx->print("Tap = zafixovat");
      gfx->setCursor(tx, ry + 17); gfx->print("Dlouhy stisk =");
      gfx->setCursor(tx, ry + 29); gfx->print("dalsi obrazovka");
    }
  }

  // Hodiny z NTP - v panelu je na ne misto a hodi se u meteoradaru.
  struct tm tmNow;
  if (getLocalTime(&tmNow, 0)) {
    char hm[8];
    snprintf(hm, sizeof(hm), "%02d:%02d", tmNow.tm_hour, tmNow.tm_min);
    UI_TextCenteredIn(hm, x0, PANEL_W, 282, C_WHITE, 2);
  }
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

// Znacky svetovych stran po obvodu kruhu rozsahu. Otaceji se spolu s mapou,
// takze je hned videt, ktery smer je nahore.
static void drawCompassMarks() {
  static const char* N4[4] = { "S", "V", "J", "Z" };
  const int r = R_RADIUS - 12;
  for (int i = 0; i < 4; i++) {
    float bearing = i * 90.0f;
    float th = (bearing - (float)s_topDeg) * 0.0174532925f;
    int x = MAP_CX + (int)lroundf(r * sinf(th));
    int y = MAP_CY - (int)lroundf(r * cosf(th));
    gfx->fillRect(x - 5, y - 6, 11, 13, C_BLACK);   // podklad kvuli citelnosti
    gfx->setTextSize(1);
    gfx->setTextColor(i == 0 ? C_RED : C_GRAY);     // sever cervene
    gfx->setCursor(x - 2, y - 3);
    gfx->print(N4[i]);
  }
}

// Pevne prvky obrazovky. Rezervuji se PRED kreslenim letadel, takze cokoli
// umisteneho daty (letadlo, volaci znak) uz jen narokuje zbytek - a kdyz je
// misto zabrane, nekresli se vubec. Pulka volaciho znaku pod legendou je horsi
// nez zadny volaci znak.
//
// Do 0.3.0 to delala funkce inReservedZone() se ctyrmi zadratovanymi
// obdelniky. Layout je totez, jen jsou souradnice odvozene od tychz konstant,
// podle kterych se ty prvky opravdu kresli - takze se to nemuze rozejit.
static void reserveChrome() {
  Layout_Begin();
  Layout_Reserve(LY_STATUS_X, LY_STATUS, LY_STATUS_W, LY_CHAR_H(1));
  Layout_Reserve(LY_LEGEND_X, LY_LEGEND, LY_LEGEND_W, LY_LEGEND_H);
  Layout_Reserve(LY_RANGE_X, LY_RANGE, LY_RANGE_W, LY_CHAR_H(1));
  Layout_Reserve(LY_RANGE_DOTS_X, LY_RANGE_DOTS_Y0, LY_RANGE_DOTS_W, LY_RANGE_DOTS_H);
  Layout_Reserve(LY_DOTS_X, LY_DOTS_Y0, LY_DOTS_W, LY_DOTS_H);
  // Pravy panel s detailem - letadla do nej nikdy nezasahuji.
  Layout_Reserve(MAP_W, 0, PANEL_W, LCD_HEIGHT);
}

void ScreenPlanes_Draw() {
  refreshRotation();
  reserveChrome();          // pevne prvky driv nez cokoli umistene daty
  gfx->fillScreen(C_BLACK);
  float range = currentRange();
  const double clat = Settings_Lat(), clon = Settings_Lon();

  // --- Podkres: hranice a mesta (stejna projekce jako letadla) ---
  //
  // Od 0.6.0 to jsou data cele Evropy (31 tisic bodu hranic, 1100 mest) misto
  // samotneho obrysu CR - podkres tim padem funguje i u hranic a v zahranici.
  // Aby to slo kreslit dost rychle, spocita se nejdriv VYREZ, ktery vubec muze
  // byt videt, a EuBorder vsechno ostatni zahodi jeste pred sahnutim do
  // framebufferu. Radar ma polomer `range` km, takze viditelny rozsah je
  // `range` na kazdou stranu; dvacetiprocentni rezerva zaridi, ze se zahodi
  // ani cara, ktera jen prochazi rohem.
  const float marginKm = range * 1.2f;
  const float dLat = marginKm / 111.0f;
  const float dLon = marginKm / (111.0f * cosf((float)clat * 0.0174532925f));
  const float lat0 = (float)clat - dLat, lat1 = (float)clat + dLat;
  const float lon0 = (float)clon - dLon, lon1 = (float)clon + dLon;

  EuBorder_Draw(cityProject, C_GRAY, lat0, lat1, lon0, lon1);
  {
    // S rostoucim rozsahem prezije mene mest - jinak by husta oblast (Poruri,
    // Horni Slezsko) pohrbila provoz pod zdi popisku.
    bool showFull = (range <= 25.0f);
    uint8_t maxTier = (range <= 50.0f) ? 3 : 2;
    // Polomer tak, aby mesta zustala v mapovem okne a nezasahla do panelu.
    int cityRad = MAP_H / 2 - 2;
    EuBorder_DrawCities(cityProject, MAP_CX, MAP_CY, cityRad,
                        C_DKGRAY, C_GRAY, showFull, maxTier,
                        lat0, lat1, lon0, lon1);
  }

  // --- Kruhy rozsahu + zamerny kriz na poloze uzivatele ---
  gfx->drawCircle(MAP_CX, MAP_CY, R_RADIUS, C_DKGRAY);
  gfx->drawCircle(MAP_CX, MAP_CY, R_RADIUS / 2, C_DKGRAY);
  gfx->drawFastHLine(MAP_CX - 8, MAP_CY, 16, C_WHITE);
  gfx->drawFastVLine(MAP_CX, MAP_CY - 8, 16, C_WHITE);
  drawCompassMarks();

  // --- Nejblizsi letadlo (fallback pro detail, kdyz nic neni vybrano) ---
  int nearestIdx = -1;
  float nearestDist = 1e9f;
  const Aircraft* list = ADSB_List();
  int n = ADSB_Count();
  for (int i = 0; i < n; i++) {
    if (list[i].lat == 0 && list[i].lon == 0) continue;
    float d = distKm(list[i].lat, list[i].lon, clat, clon);
    if (d < nearestDist) { nearestDist = d; nearestIdx = i; }
  }

  // Letadlo do detailu: rucne vybrane (dle ICAO), jinak automaticky nejblizsi.
  int selIdx = (s_selHex[0]) ? ADSB_FindByHex(s_selHex) : -1;
  int detailIdx = (selIdx >= 0) ? selIdx : ((s_selHex[0] && s_selCacheOk) ? -1 : nearestIdx);

  // --- Vykresleni letadel (jen v mapovem okne) ---
  int shown = 0;
  s_planeN = 0;                        // znovu naplnime pozice pro tap-to-select
  for (int i = 0; i < n; i++) {
    if (list[i].lat == 0 && list[i].lon == 0) continue;
    int sx, sy;
    project(list[i].lat, list[i].lon, clat, clon, range, &sx, &sy);
    if (sx < 2 || sx >= MAP_W - 2 || sy < 2 || sy >= MAP_H - 2) continue;  // mimo mapu
    // Ikona letadla je ~13 px; kdyz jeji misto neni volne, lezi pod pevnym
    // prvkem (legenda, rozsah, tecky, panel) a nekresli se.
    //
    // Zamerne se jen TESTUJE, nezabira: dve letadla nad sebou se prekryvaji
    // i ve skutecnosti a obe tam patri. Vyhradni pravo na misto potrebuji az
    // popisky nize, ktere by jinak byly necitelne.
    if (!Layout_IsFree(sx - 8, sy - 8, 17, 17)) continue;

    // Ulozit pozici + ICAO pro dotykovy vyber.
    if (s_planeN < ADSB_MAX) {
      s_planeX[s_planeN] = sx; s_planeY[s_planeN] = sy;
      strncpy(s_planeHex[s_planeN], list[i].hex, sizeof(s_planeHex[0]) - 1);
      s_planeHex[s_planeN][sizeof(s_planeHex[0]) - 1] = '\0';
      s_planeN++;
    }

    if (i == detailIdx) {   // zvyrazneni letadla v detailu bilym krouzkem
      gfx->drawCircle(sx, sy, 15, C_WHITE);
      gfx->drawCircle(sx, sy, 17, C_WHITE);
    }
    // Kurz se opravuje o otoceni mapy - ikona pak miri tam, kam letadlo leti.
    float trackOnScreen = list[i].track - (float)s_topDeg;
    drawPlane(sx, sy, trackOnScreen, list[i].hasTrack, altColor(list[i].altFt));
    if (list[i].callsign[0]) {
      const int tw = Layout_TextW(list[i].callsign, 1);
      int lx = sx - tw / 2;
      if (lx < 1) lx = 1;
      if (lx + tw > MAP_W - 2) lx = MAP_W - 2 - tw;
      // Popisek se kresli, jen kdyz je pro nej misto. Pri hustem provozu se
      // jich par vynecha - to je zamer, prekryvajici se znacky se stejne
      // neprectou.
      if (Layout_Claim(lx - 1, sy + 15, tw + 2, LY_CHAR_H(1) + 2)) {
        gfx->setTextSize(1); gfx->setTextColor(C_WHITE);
        gfx->setCursor(lx, sy + 16);
        gfx->print(list[i].callsign);
      }
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
    snprintf(sub, sizeof(sub), "%d %s", shown, T(S_AIRCRAFT));
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
  const Aircraft* det = nullptr;
  float detailDist = 0.0f;
  bool  stale = false;
  if (detailIdx >= 0) {
    det = &list[detailIdx];
  } else if (s_selHex[0] && s_selCacheOk) {
    // Grace perioda: letadlo v poslednim stazeni chybelo, ukazeme posledni
    // zname hodnoty misto toho, aby detail uskocil jinam.
    det = &s_selCache;
    stale = true;
  }
  if (det) detailDist = distKm(det->lat, det->lon, clat, clon);
  // Na trasu se zeptame az tady, kdyz uz je jasne, ktere letadlo je v detailu.
  // Route_Select() je levne a idempotentni - opakovane volani s tymz volacim
  // znakem uz nic nedela, jakmile je odpoved v kesi. Samotne stazeni probehne
  // az v Route_Tick() z loop(), aby se odsud nesitovalo.
  if (det) Route_Select(det->callsign, det->hex);
  else     Route_Clear();

  drawPanel(det, detailDist, s_selHex[0] != '\0', stale);
}
