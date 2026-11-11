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
#include "Lang.h"     // LT(): cesky / slovensky, s diakritikou i bez
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
#include "PlaneIcon.h"
#include "Geo.h"

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

// Barva letadla podle nadmorske vysky (vzdy ve stopach, nezavisle na
// jednotkach). Hranice a barvy jsou od 0.8.0 v nastaveni (zalozka Letadla);
// vychozi hodnoty jsou presne puvodni legenda teto desky.
static uint16_t altColor(float altFt) {
  if (altFt < 1) return C_GRAY;           // vyska neznama
  for (uint8_t i = 0; i < ALT_BANDS_N; i++)
    if (altFt < (float)Settings_AltBandFt(i)) return Settings_AltColor(i);
  return Settings_AltColor(ALT_BANDS_N);
}

// Ikona letadla podle typu (0.8.0, PlaneIcon.h) natocena podle kurzu. Bez
// kurzu krouzek s teckou. trackDeg uz musi byt OPRAVENY o otoceni mapy.
static void drawPlane(int x, int y, float trackDeg, bool hasTrack, uint8_t kind, uint16_t col) {
  PlaneIcon_Draw(x, y, trackDeg, hasTrack, kind, col);
}

// --- Upozorneni na letadlo poblic (0.8.0) -------------------------------------
static bool  s_alert = false;
static bool  s_alertWindow = false;
static float s_alertKm = 0;

void ScreenPlanes_AlertBegin(const char* hex, float distKm, bool inWindow) {
  s_alert = true;
  s_alertWindow = inWindow;
  s_alertKm = distKm;
  // Nejmensi rozsah, do ktereho se letadlo vejde i s rezervou.
  int idx = RANGE_COUNT - 1;
  for (int i = 0; i < RANGE_COUNT; i++) {
    if (RANGES_KM[i] >= distKm * 1.15f) { idx = i; break; }
  }
  s_rangeIdx = idx;
  s_nextFetch = 0;
  if (hex && *hex) selectHex(hex);
}

void ScreenPlanes_AlertEnd() {
  if (!s_alert) return;
  s_alert = false;
  selectNone("konec upozorneni");
  s_rangeIdx = Settings_PlaneRange();
  if (s_rangeIdx >= RANGE_COUNT) s_rangeIdx = 1;
  s_nextFetch = 0;
}

bool ScreenPlanes_AlertActive() { return s_alert; }

void ScreenPlanes_Enter() {
  s_rangeIdx = Settings_PlaneRange();
  if (s_rangeIdx >= RANGE_COUNT) s_rangeIdx = 1;   // ochrana proti stare hodnote
  s_nextFetch = 0;
  refreshRotation();
}

bool ScreenPlanes_Tick() {
  if (WiFi.status() != WL_CONNECTED) { s_status = T(S_WIFI_WAIT); return false; }

  // Odpoved na trasu prijde nezavisle na stahovani letadel, obvykle do vteriny
  // po klepnuti. Bez tohohle by se dokreslila az s pristim pollem, tedy podle
  // rozsahu o nekolik sekund pozdeji, a panel by mezitim zbytecne ukazoval
  // "zjistuji trasu...". Ctenim priznak zhasne, takze se prekresli jednou.
  bool routeChanged = Route_TakeChanged();

  if (millis() >= s_nextFetch) {
    s_status = T(S_DOWNLOADING);
    s_dataOk = ADSB_Fetch(Settings_Lat(), Settings_Lon(), currentRange());
    s_status = s_dataOk ? "OK" : T(S_ERROR);

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
  return routeChanged;   // jinak kreslit jen kdyz dorazila trasa
}

void ScreenPlanes_ChangeRange(int dir) {
  s_alert = false;
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

// Orizne radek na maxCh znaku a konec oznaci teckou, at nic nepretece do mapy.
static void clipTo(char* s, int maxCh) {
  if (maxCh < 2) return;
  if ((int)strlen(s) > maxCh) { s[maxCh - 1] = '.'; s[maxCh] = '\0'; }
}

static void drawPanel(const Aircraft* ac, float distKmVal, bool pinned, bool stale) {
  const int x0 = MAP_W;
  gfx->fillRect(x0, 0, PANEL_W, LCD_HEIGHT, C_DKGRAY);     // pozadi panelu
  gfx->drawFastVLine(x0, 0, LCD_HEIGHT, C_CYAN);           // oddelovaci cara

  const int tx = x0 + 6;
  const bool metric = Settings_MetricUnits();

  gfx->setTextSize(1); gfx->setTextColor(C_CYAN);
  gfx->setCursor(tx, 6);
  gfx->print(pinned ? LT("VYBRANE", "VYBRANÉ", "VYBRATE", "VYBRATÉ")
                    : LT("NEJBLIZSI", "NEJBLIŽŠÍ", "NAJBLIZSIE", "NAJBLIŽŠIE"));

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
    // Bez volacky se ukaze ICAO adresa. Ten zaloznik zije TADY a nikde jinde -
    // Aircraft::callsign se schvalne necha prazdny, protoze prave on jde do
    // dotazu na trasu a hex tam ctou jako cislo letu (viz copyCallsign
    // v ADSB.cpp). Vykreslit ho je neskodne, ptat se na nej ne.
    gfx->print(ac->callsign[0] ? ac->callsign : (ac->hex[0] ? ac->hex : "?"));

    // Pod volackou je jeden radek: ICAO hex, nebo misto nej upozorneni.
    // Driv slo upozorneni na y=52 a zasahovalo do popisku "Typ" (y=58).
    gfx->setTextSize(1);
    if (stale) {
      // Data uz nejsou cerstva (grace perioda).
      gfx->setTextColor(C_YELLOW);
      gfx->setCursor(tx, 38); gfx->print(T(S_SIGNAL_LOST));
    } else if (s_alert) {
      // Proc je obrazovka na displeji: letadlo poblic nebo ve vyhledu z okna
      // (0.8.0). Vzdalenost se pocita znovu, letadlo se mezitim hybe.
      float dkm = s_alertKm;
      Geo_DistBearing(Settings_Lat(), Settings_Lon(), ac->lat, ac->lon, &dkm, nullptr);
      char note[32];
      snprintf(note, sizeof(note), "%s %.1f km",
               s_alertWindow ? T(S_IN_WINDOW) : T(S_NEARBY), dkm);
      gfx->setTextColor(C_CYAN);
      gfx->setCursor(tx, 38); gfx->print(note);
    } else {
      // ICAO hex - podle nej se letadlo drzi mezi stazenimi.
      gfx->setTextColor(C_GRAY);
      gfx->setCursor(tx, 38); gfx->print(ac->hex);
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
      char label[24] = "", value[12] = "", unit[8] = "";
      switch (row) {
        case 0:
          snprintf(label, sizeof(label), "%s", T(S_TYPE));
          snprintf(value, sizeof(value), "%s", ac->type[0] ? ac->type : "-");
          break;
        case 1:
          snprintf(label, sizeof(label), "%s", LT("Vzdalenost", "Vzdálenost", "Vzdialenost", "Vzdialenosť"));
          snprintf(value, sizeof(value), "%.1f", distKmVal);
          snprintf(unit, sizeof(unit), "km");
          break;
        case 2:
          snprintf(label, sizeof(label), "%s", T(S_ALTITUDE));
          if (metric) { snprintf(value, sizeof(value), "%.0f", ac->altFt * 0.3048f);
                        snprintf(unit, sizeof(unit), "m"); }
          else        { snprintf(value, sizeof(value), "%.0f", ac->altFt);
                        snprintf(unit, sizeof(unit), "ft"); }
          break;
        case 3:
          snprintf(label, sizeof(label), "%s", T(S_SPEED));
          if (metric) { snprintf(value, sizeof(value), "%.0f", ac->gsKt * 1.852f);
                        snprintf(unit, sizeof(unit), "km/h"); }
          else        { snprintf(value, sizeof(value), "%.0f", ac->gsKt);
                        snprintf(unit, sizeof(unit), "kt"); }
          break;
        case 4:
          snprintf(label, sizeof(label), "%s", T(S_TRACK));
          if (ac->hasTrack) { snprintf(value, sizeof(value), "%.0f", ac->track);
                              snprintf(unit, sizeof(unit), "deg"); }
          else                snprintf(value, sizeof(value), "?");
          break;
        default:
          snprintf(label, sizeof(label), "%s", T(S_CLIMB));
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
      int vw = Layout_TextW(value, (uint8_t)vSize);
      const int uw = Layout_TextW(unit, 1);
      if (vw + (unit[0] ? uw + 3 : 0) > avail) { vSize = 1; vw = Layout_TextW(value, 1); }

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

  // --- Odkud a kam leti (adsb.lol) ---
  //
  // Pta se jen na VYBRANE letadlo, ne na cely seznam, a odpoved se kesuje.
  // Spousta letu zadnou trasu nema (vseobecne letectvi, vojaci, vrtulniky)
  // a spousta letadel nevysila volacku - oboji je normalni vysledek, ne chyba,
  // a proste se nic neukaze. Navic server trasu overuje proti poloze letadla,
  // takze ta, ktera k mistu nesedi, sem uz vubec nedojde.
  //
  // Do panelu sirokeho 124 px se vejde 20 znaku pri velikosti 1. adsb.lol vraci
  // nazvy mest ("Prague"), ktere se ctou lip nez kody, ale "Frankfurt am Main"
  // by pretekl do mapy - dlouhy nazev se proto orizne teckou.
  gfx->drawFastHLine(x0 + 4, PANEL_SEP_Y, PANEL_W - 8, C_GRAY);
  {
    int ry = PANEL_SEP_Y + 8;
    const RouteState rs = Route_GetState();
    if (rs == ROUTE_WAIT) {
      gfx->setTextSize(1); gfx->setTextColor(C_DKGRAY);
      gfx->setCursor(tx, ry); gfx->print(LT("zjistuji trasu...", "zjišťuji trasu...", "zistujem trasu...", "zisťujem trasu..."));
      ry += 14;
    } else if (rs == ROUTE_OK) {
      const RouteInfo* rt = Route_Get();
      char line[32];
      gfx->setTextSize(1);
      // Pri velikosti 1 je znak 6 px siroky, panel ma PANEL_W - 8 px na text.
      const int maxCh = (PANEL_W - 8) / 6;
      if (rt->from[0]) {
        gfx->setTextColor(C_WHITE);
        snprintf(line, sizeof(line), "Z:  %s", rt->from);
        clipTo(line, maxCh);
        gfx->setCursor(tx, ry); gfx->print(line); ry += 13;
      }
      if (rt->to[0]) {
        gfx->setTextColor(C_WHITE);
        snprintf(line, sizeof(line), "Do: %s", rt->to);
        clipTo(line, maxCh);
        gfx->setCursor(tx, ry); gfx->print(line); ry += 13;
      }
    }
    // Registrace pod trasou. Uz nechodi z databaze trasy, ale primo z adsb.fi
    // (pole "r"), takze se ukaze i u letadla, ktere zadnou trasu nema.
    if (ac && ac->reg[0]) {
      gfx->setTextSize(1); gfx->setTextColor(C_GRAY);
      gfx->setCursor(tx, ry); gfx->print(ac->reg); ry += 13;
    }

    // Napoveda k ovladani se kresli az pod trasou a jen kdyz na ni zbylo misto -
    // trasa je informace, napoveda jen pripominka.
    if (ry <= 268) {
      gfx->setTextSize(1); gfx->setTextColor(C_DKGRAY);
      gfx->setCursor(tx, ry + 4);  gfx->print(LT("Tap = zafixovat", "Tap = zafixovat", "Tap = zafixovat", "Tap = zafixovať"));
      gfx->setCursor(tx, ry + 17); gfx->print(LT("Dlouhy stisk =", "Dlouhý stisk =", "Dlhy stisk =", "Dlhý stisk ="));
      gfx->setCursor(tx, ry + 29); gfx->print(LT("dalsi obrazovka", "další obrazovka", "dalsia obrazovka", "ďalšia obrazovka"));
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

// Legenda barev letadel podle vysky (vlevo na mape). Od 0.8.0 se sklada
// z nastaveni, odshora nejvyssi pasmo. Hranice jsou ulozene ve stopach;
// s metrickymi jednotkami se legenda pise v kilometrech, jinak v tisicich
// stop - stejne jako vyska v detailu letadla.
static void drawLegend() {
  const int lx = 4, ly = 20;
  const bool metric = Settings_MetricUnits();
  gfx->setTextSize(1); gfx->setTextColor(C_GRAY);
  gfx->setCursor(lx, ly);
  gfx->print(metric ? LT("vyska km", "výška km", "vyska km", "výška km") : "FL x1000ft");
  char lbl[ALT_BANDS_N + 2][12];
  uint16_t col[ALT_BANDS_N + 2];
  auto k = [metric](uint16_t ft, char* out, size_t cap) {
    if (metric) {
      // na desetiny km; cela cisla bez ",0" (3 km, ne 3.0 km)
      const int d = (int)lroundf(ft * 0.003048f);          // desetiny km
      if (d % 10 == 0) snprintf(out, cap, "%d", d / 10);
      else             snprintf(out, cap, "%d.%d", d / 10, d % 10);
    } else if (ft % 1000 == 0) snprintf(out, cap, "%u", (unsigned)(ft / 1000));
    else                       snprintf(out, cap, "%.1f", ft / 1000.0f);
  };
  char a[8], b[8];
  // Odshora: nad posledni hranici, pasma mezi, pod prvni, neznama vyska.
  k(Settings_AltBandFt(ALT_BANDS_N - 1), a, sizeof(a));
  snprintf(lbl[0], sizeof(lbl[0]), ">%s", a);
  col[0] = Settings_AltColor(ALT_BANDS_N);
  for (int i = ALT_BANDS_N - 1, r = 1; i >= 1; i--, r++) {
    k(Settings_AltBandFt(i - 1), a, sizeof(a));
    k(Settings_AltBandFt(i), b, sizeof(b));
    snprintf(lbl[r], sizeof(lbl[r]), "%s-%s", a, b);
    col[r] = Settings_AltColor(i);
  }
  k(Settings_AltBandFt(0), a, sizeof(a));
  snprintf(lbl[ALT_BANDS_N], sizeof(lbl[0]), "<%s", a);
  col[ALT_BANDS_N] = Settings_AltColor(0);
  snprintf(lbl[ALT_BANDS_N + 1], sizeof(lbl[0]), "?");
  col[ALT_BANDS_N + 1] = C_GRAY;
  for (int i = 0; i < ALT_BANDS_N + 2; i++) {
    int ry = ly + 12 + i * 13;
    gfx->fillRect(lx, ry, 9, 9, col[i]);
    gfx->drawRect(lx, ry, 9, 9, C_DKGRAY);
    gfx->setTextColor(C_WHITE);
    gfx->setCursor(lx + 13, ry + 1);
    gfx->print(lbl[i]);
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

  // --- Vyhled z okna (0.8.0): dve tenke cary a teckovany oblouk na dohledu ---
  if (Settings_WindowOn()) {
    const float half = Settings_WindowWidth() * 0.5f;
    const float a0 = (float)Settings_WindowAz() - half;
    const float a1 = (float)Settings_WindowAz() + half;
    float rr = (float)R_RADIUS * (float)Settings_WindowKm() / range;
    if (rr > R_RADIUS) rr = R_RADIUS;
    const uint16_t wc = 0x0339;
    for (int e = 0; e < 2; e++) {
      const float bb = ((e ? a1 : a0) - (float)s_topDeg) * 0.0174532925f;
      gfx->drawLine(MAP_CX, MAP_CY, MAP_CX + (int)(rr * sinf(bb)), MAP_CY - (int)(rr * cosf(bb)), wc);
    }
    for (float d = a0; d <= a1; d += 3.0f) {
      const float bb = (d - (float)s_topDeg) * 0.0174532925f;
      gfx->drawPixel(MAP_CX + (int)(rr * sinf(bb)), MAP_CY - (int)(rr * cosf(bb)), wc);
    }
  }

  // --- Nejblizsi letadlo (fallback pro detail, kdyz nic neni vybrano) ---
  int nearestIdx = -1;
  float nearestDist = 1e9f;
  const Aircraft* list = ADSB_List();
  int n = ADSB_Count();
  // Stara data se nevydavaji za ziva (0.8.4): po minute bez cerstvych dat jsou
  // letadla seda, po deseti minutach se nekresli vubec.
  const bool staleData = ADSB_DataStale();
  if (ADSB_DataHidden()) n = 0;
  for (int i = 0; i < n; i++) {
    if (list[i].lat == 0 && list[i].lon == 0) continue;
    float d = distKm(list[i].lat, list[i].lon, clat, clon);
    if (d < nearestDist) { nearestDist = d; nearestIdx = i; }
  }

  // Letadlo do detailu: rucne vybrane (dle ICAO), jinak automaticky nejblizsi.
  int selIdx = (s_selHex[0]) ? ADSB_FindByHex(s_selHex) : -1;
  int detailIdx = (selIdx >= 0) ? selIdx : ((s_selHex[0] && s_selCacheOk) ? -1 : nearestIdx);

  // --- Vykresleni letadel (jen v mapovem okne) ---
  //
  // Dva pruchody. V prvnim se zjisti, kde ktere letadlo je, a nakresli se
  // ikony - vybrane letadlo s krouzkem az nakonec, aby ho jine neprekrylo.
  // Ve druhem se hledaji mista pro popisky: s odstupem podle skutecne
  // velikosti ikony (A380 je skoro dvakrat vetsi nez Cessna, krouzek vybraneho
  // ma polomer 17 px) a mimo ikony VSECH letadel, ne jen toho sveho. Driv sel
  // popisek vzdy 16 px pod stred, takze u velkych a hlavne vybranych letadel
  // lezel v ikone a v krouzku.
  static int vIdx[ADSB_MAX], vX[ADSB_MAX], vY[ADSB_MAX], vR[ADSB_MAX];
  int vn = 0, vDetail = -1;
  s_planeN = 0;                        // znovu naplnime pozice pro tap-to-select
  for (int i = 0; i < n && vn < ADSB_MAX; i++) {
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
    int r = PlaneIcon_Radius(list[i].icon, list[i].hasTrack);
    if (i == detailIdx) { vDetail = vn; if (r < 18) r = 18; }   // krouzek 15 + 17 px
    vIdx[vn] = i; vX[vn] = sx; vY[vn] = sy; vR[vn] = r;
    vn++;
  }

  auto drawIcon = [&](int k) {
    const Aircraft& a = list[vIdx[k]];
    if (k == vDetail) {   // zvyrazneni letadla v detailu bilym krouzkem
      gfx->drawCircle(vX[k], vY[k], 15, C_WHITE);
      gfx->drawCircle(vX[k], vY[k], 17, C_WHITE);
    }
    // Kurz se opravuje o otoceni mapy - ikona pak miri tam, kam letadlo leti.
    float trackOnScreen = a.track - (float)s_topDeg;
    drawPlane(vX[k], vY[k], trackOnScreen, a.hasTrack, a.icon,
              staleData ? C_GRAY : altColor(a.altFt));
  };
  for (int k = 0; k < vn; k++) if (k != vDetail) drawIcon(k);
  if (vDetail >= 0) drawIcon(vDetail);

  // Zasahuje obdelnik popisku do ikony nektereho letadla?
  auto hitsIcon = [&](int x, int y, int w, int h) {
    for (int k = 0; k < vn; k++) {
      const int r = vR[k];
      if (x < vX[k] + r && x + w > vX[k] - r && y < vY[k] + r && y + h > vY[k] - r) return true;
    }
    return false;
  };
  auto placeLabel = [&](int k) {
    const Aircraft& a = list[vIdx[k]];
    if (!a.callsign[0]) return;
    const int tw = Layout_TextW(a.callsign, 1), th = LY_CHAR_H(1);
    const int gap = vR[k] + 2;
    const int cx = vX[k] - tw / 2, sx = vX[k], sy = vY[k];
    // Pod ikonou, nad ni, vpravo, vlevo - prvni misto, ktere je volne.
    const int cand[4][2] = { { cx, sy + gap }, { cx, sy - gap - th },
                             { sx + gap, sy - th / 2 }, { sx - gap - tw, sy - th / 2 } };
    for (int c = 0; c < 4; c++) {
      int lx = cand[c][0];
      const int ly = cand[c][1];
      if (c < 2) {                                   // nad/pod: posunout do mapy
        if (lx < 1) lx = 1;
        if (lx + tw > MAP_W - 2) lx = MAP_W - 2 - tw;
      } else if (lx < 1 || lx + tw > MAP_W - 2) continue;
      if (ly < 1 || ly + th > MAP_H - 1) continue;
      if (hitsIcon(lx - 1, ly - 1, tw + 2, th + 2)) continue;
      // Popisek se kresli, jen kdyz je pro nej misto. Pri hustem provozu se
      // jich par vynecha - to je zamer, prekryvajici se znacky se stejne
      // neprectou.
      if (!Layout_Claim(lx - 1, ly - 1, tw + 2, th + 2)) continue;
      gfx->setTextSize(1); gfx->setTextColor(C_WHITE);
      gfx->setCursor(lx, ly);
      gfx->print(a.callsign);
      return;
    }
  };
  if (vDetail >= 0) placeLabel(vDetail);           // vybrane letadlo ma prednost
  for (int k = 0; k < vn; k++) if (k != vDetail) placeLabel(k);
  const int shown = vn;

  // --- Pocet letadel / stav: maly font, vlevo nahore ---
  gfx->setTextSize(1);
  if (WiFi.status() == WL_CONNECTED && staleData && ADSB_DataAgeMs() != 0xFFFFFFFFUL) {
    // Kolik jsou data stara a jak dlouho jeste trva pauza po chybach.
    char sub[48];
    const unsigned long ageMin = ADSB_DataAgeMs() / 60000UL;
    const unsigned long pause = ADSB_PauseLeftMs() / 1000UL;
    if (ADSB_DataHidden()) snprintf(sub, sizeof(sub), "%s", T(S_NO_PLANE_DATA));
    else snprintf(sub, sizeof(sub), "%s: %lu min", T(S_OLD_PLANES), ageMin ? ageMin : 1UL);
    if (pause) {
      const size_t l = strlen(sub);
      snprintf(sub + l, sizeof(sub) - l, " (%s %lu s)", T(S_PAUSE), pause);
    }
    gfx->setTextColor(C_YELLOW);
    gfx->setCursor(4, 4); gfx->print(sub);
  } else if (WiFi.status() != WL_CONNECTED || !s_dataOk) {
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
  // Poloha jde s dotazem: server podle ni odmitne trasu, ktera k mistu, kde
  // letadlo opravdu je, nesedi. Prave to drive delalo z letadla nad Prahou let
  // Atheny - Istanbul, kdyz se cislo letu recyklovalo.
  if (det) Route_Select(det->callsign, det->lat, det->lon);
  else     Route_Clear();

  drawPanel(det, detailDist, s_selHex[0] != '\0', stale);
}
