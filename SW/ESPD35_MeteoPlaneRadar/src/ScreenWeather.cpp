// =============================================================================
//  ESPD35_MeteoPlaneRadar - obrazovka meteoradaru CHMU s ANIMACI (6 ramcu).
//  Port z petus/Meteo-PlaneRadar na obdelnik 480x320.
//
//  Souradnice: Web Mercator, vyrez s pomerem stran 1.5:1 (izotropni meritko).
//  Animace: 6 ramcu (-25 .. nyni), 2 snimky/s, mezi cykly ~5 s pauza.
//  Nove ramce se stahuji jen kdyz se zobrazuje posledni snimek (v pauze),
//  aby se animace nepresekavala. Behem stahovani "Nacitam animaci...".
// =============================================================================
#include "Lang.h"     // LT(): cesky / slovensky, s diakritikou i bez
#include "ScreenWeather.h"
#include "EuBorder.h"
#include "Lang.h"
#include "Layout.h"
#include "CHMU.h"
#include "Settings.h"
#include "UI.h"
#include "Config.h"
#include "EuBorder.h"

#include <WiFi.h>
#include <PNGdec.h>
#include <math.h>
#include <esp_heap_caps.h>
#include <string.h>

#define ANIM_FRAMES  CHMU_ANIM_MAX   // 6

// Rozsahy meteoradaru (polomer na VYSKU v km) - z Config.h.
static const float RANGES_KM[] = METEO_RANGES_KM;
static const int   RANGE_COUNT = sizeof(RANGES_KM) / sizeof(RANGES_KM[0]);
static int s_rangeIdx = 1;
static float currentRange() { return RANGES_KM[s_rangeIdx]; }

// Rozsah 0 je zvlastni: znamena PEVNY pohled na celou CR bez ohledu na to,
// kde uzivatel je (viz CZ_VIEW_* v Config.h). Zadava se jako stred + polomer,
// ne jako obdelnik, protoze obrazek CHMU ma na kazde ose jine meritko - ram,
// ktery vypada ctvercove v pixelech, ctvercovy NA ZEMI neni a stat by se
// svisle protahl. Pruchodem tymz vypoctem jako u ostatnich rozsahu zustane
// meritko poctive.
static bool  wholeCountry()  { return currentRange() <= 0.0f; }
static double viewLat()      { return wholeCountry() ? (double)CZ_VIEW_LAT : Settings_Lat(); }
static double viewLon()      { return wholeCountry() ? (double)CZ_VIEW_LON : Settings_Lon(); }
static float  viewRadiusKm() { return wholeCountry() ? CZ_VIEW_RADIUS_KM : currentRange(); }

// POZNAMKA: meteoradar se zamerne NEOTACI podle nastaveni "Nahore". Srazkova
// mapa se cte severem nahoru a orientaci v ni drzi obrys CR. Otoceni plati jen
// pro radar letadel, ktery je "pohled z mista, kde stojim".

static PNG png;
static int s_imgW = 600, s_imgH = 480;
static int s_dataX1 = 100000, s_dataY0 = -1;   // hranice DAT (mimo = titulek/skala)
static String s_status = "Start...";
static bool s_wide = false;              // snimek moc siroky pro PNGdec
static bool s_stale = false;             // posledni stazeni selhalo, zobrazena starsi sada

// Kdy se ma zkusit dalsi stazeni. Po uspechu za METEO_PERIOD_MS, po neuspechu
// uz za METEO_RETRY_MS. Drive se pri prazdne animaci zkousel index CHMU v kazdem
// pruchodu smyckou, tedy kazdou sekundu, a po vypadku to tak jelo dokola.
static unsigned long s_fetchEvery = 0;

struct Crop { int x1, y1, x2, y2; };
static Crop s_crop;     // POHLED: vystredeny na uzivatele, muze presahovat snimek
static Crop s_clip;     // jeho prunik se snimkem - jen ten se dekoduje a uklada
static uint16_t* s_lineBuf = nullptr;
static int       s_lineCap = 0;

// Buffery ramcu (dekodovane vyrezy) v PSRAM.
static uint16_t* s_frame565[ANIM_FRAMES] = {0};
static int       s_frameCap[ANIM_FRAMES] = {0};
static uint16_t* s_crop565 = nullptr;    // aktualni cil (pngDraw) / zdroj (blit)
static int       s_frameCount = 0;

// Animacni stav.
static int  s_curFrame = 0;
static bool s_gap = false;
static bool s_needRebuild = false;
static unsigned long s_lastStep = 0, s_gapStart = 0, s_lastFetch = 0;

static int clampI(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// --- Web Mercator ---
static float mercY(float latDeg) { float r = latDeg * 0.0174532925f; return logf(tanf(0.7853981634f + r * 0.5f)); }
static float mercTop()    { return mercY(CHMU_LAT_TOP); }
static float mercBottom() { return mercY(CHMU_LAT_BOTTOM); }
static int lonToX(float lon) { return lroundf((lon - CHMU_LON_LEFT) * (s_imgW - 1) / (CHMU_LON_RIGHT - CHMU_LON_LEFT)); }
static int latToY(float lat) { float yt = mercTop(), yb = mercBottom(); return lroundf((yt - mercY(lat)) * (s_imgH - 1) / (yt - yb)); }

static const float ASPECT = (float)LCD_WIDTH / (float)LCD_HEIGHT;   // 1.5

static void makeCrop(double lat, double lon, float radiusKm) {
  float degLat = radiusKm / 111.32f;
  float degLon = radiusKm * ASPECT / (111.32f * cosf(lat * 0.0174532925));
  int x1 = lonToX(lon - degLon), x2 = lonToX(lon + degLon);
  int y1 = latToY(lat + degLat), y2 = latToY(lat - degLat);
  if (x2 < x1) { int t = x1; x1 = x2; x2 = t; }
  if (y2 < y1) { int t = y1; y1 = y2; y2 = t; }
  // POHLED se NEOREZAVA na okraj snimku. Orez ho prestal centrovat na
  // uzivatele, jenze zamerny kriz se dal kreslil doprostred displeje - u Chebu
  // na 200 km to delalo pres 80 km stranou, u Ostravy kolem 10 km. Cast mimo
  // snimek se vykresli cerne (blitCrop) a kriz tak vzdy sedi na skutecnou polohu.
  //
  // Ulozi se ale jen PRUNIK se snimkem (s_clip): sest ramcu celeho pohledu
  // "cela CR" by v PSRAM zabralo kolem 5 MB, z toho velkou cast cerne.
  s_crop.x1 = x1;
  s_crop.x2 = x2;
  s_crop.y1 = y1;
  s_crop.y2 = y2;
  s_clip.x1 = clampI(x1, 0, s_imgW - 1);
  s_clip.x2 = clampI(x2, 0, s_imgW - 1);
  s_clip.y1 = clampI(y1, 0, s_imgH - 1);
  s_clip.y2 = clampI(y2, 0, s_imgH - 1);
}
static int cropW() { return s_crop.x2 - s_crop.x1 + 1; }
static int cropH() { return s_crop.y2 - s_crop.y1 + 1; }
static int clipW() { return s_clip.x2 - s_clip.x1 + 1; }
static int clipH() { return s_clip.y2 - s_clip.y1 + 1; }

static void borderProject(float lat, float lon, int* sx, int* sy) {
  int srcX = lonToX(lon), srcY = latToY(lat);
  *sx = (int)((int64_t)(srcX - s_crop.x1) * LCD_WIDTH  / cropW());
  *sy = (int)((int64_t)(srcY - s_crop.y1) * LCD_HEIGHT / cropH());
}

static int pngDraw(PNGDRAW* d) {
  int srcY = d->y;
  if (srcY < s_clip.y1 || srcY > s_clip.y2) return 1;
  if (!s_crop565 || !s_lineBuf) return 1;
  png.getLineAsRGB565(d, s_lineBuf, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
  const int cw = clipW();
  uint16_t* row = s_crop565 + (int64_t)(srcY - s_clip.y1) * cw;
  for (int i = 0; i < cw; i++) {
    const int srcX = s_clip.x1 + i;
    // Mimo oblast dat (titulek nahore, skala vpravo v PNG) = cerna.
    row[i] = (srcX > s_dataX1 || srcY < s_dataY0) ? 0x0000 : s_lineBuf[srcX];
  }
  return 1;
}

// Roztahne POHLED pres celou obrazovku (nearest neighbour). Ulozeny je jen
// jeho prunik se snimkem (s_clip); co lezi mimo, je cerne.
static void blitCrop() {
  if (!s_crop565) return;
  const int cw = cropW(), ch = cropH();
  const int kw = clipW();
  for (int dy = 0; dy < LCD_HEIGHT; dy++) {
    const int srcY = s_crop.y1 + (int)((int64_t)dy * ch / LCD_HEIGHT);
    if (srcY < s_clip.y1 || srcY > s_clip.y2) {
      gfx->drawFastHLine(0, dy, LCD_WIDTH, 0x0000);
      continue;
    }
    const uint16_t* row = s_crop565 + (int64_t)(srcY - s_clip.y1) * kw;
    for (int dx = 0; dx < LCD_WIDTH; dx++) {
      const int srcX = s_crop.x1 + (int)((int64_t)dx * cw / LCD_WIDTH);
      gfx->drawPixel(dx, dy, (srcX < s_clip.x1 || srcX > s_clip.x2)
                             ? 0x0000 : row[srcX - s_clip.x1]);
    }
  }
}

// -----------------------------------------------------------------------------
//  Sestaveni vyrezu vsech ramcu (dekodovani PNG -> s_frame565[f]).
// -----------------------------------------------------------------------------
static bool rebuildCrops() {
  int cnt = CHMU_AnimCount();
  if (cnt <= 0) { s_frameCount = 0; return false; }
  if (cnt > ANIM_FRAMES) cnt = ANIM_FRAMES;

  // Rozmery + kontrola sirky z prvniho ramce.
  if (png.openRAM(CHMU_AnimData(0), CHMU_AnimSize(0), pngDraw) != PNG_SUCCESS) { s_frameCount = 0; return false; }
  s_imgW = png.getWidth(); s_imgH = png.getHeight();
  s_dataX1 = lonToX(CHMU_LON_DATA_RIGHT);   // za timto x je vpravo skala/okraj
  s_dataY0 = latToY(CHMU_LAT_DATA_TOP);     // nad timto y je nahore titulek
  int bufSize = png.getBufferSize();
  int pitch = (s_imgH > 0) ? bufSize / s_imgH : bufSize;
  png.close();
  if (2 * (pitch + 1) > (int)PNG_MAX_BUFFERED_PIXELS) { s_wide = true; s_frameCount = 0; return false; }
  s_wide = false;

  if (s_imgW > s_lineCap) {
    if (s_lineBuf) free(s_lineBuf);
    s_lineBuf = (uint16_t*)malloc((size_t)s_imgW * sizeof(uint16_t));
    s_lineCap = s_lineBuf ? s_imgW : 0;
  }
  if (!s_lineBuf) { s_frameCount = 0; return false; }

  makeCrop(viewLat(), viewLon(), viewRadiusKm());
  int need = clipW() * clipH();

  int okc = 0;
  for (int f = 0; f < cnt; f++) {
    if (s_frameCap[f] < need) {
      if (s_frame565[f]) free(s_frame565[f]);
      s_frame565[f] = (uint16_t*)heap_caps_malloc((size_t)need * 2, MALLOC_CAP_SPIRAM);
      if (!s_frame565[f]) s_frame565[f] = (uint16_t*)malloc((size_t)need * 2);
      s_frameCap[f] = s_frame565[f] ? need : 0;
    }
    if (!s_frame565[f]) break;
    s_crop565 = s_frame565[f];
    if (png.openRAM(CHMU_AnimData(f), CHMU_AnimSize(f), pngDraw) != PNG_SUCCESS) { png.close(); break; }
    png.decode(nullptr, 0);
    png.close();
    okc++;
  }
  s_frameCount = okc;
  return okc > 0;
}

// Stazeni ramcu + sestaveni (blokujici).
//
// Kdyz uz nejaka animace je, displej NEZCERNA. Drive se kazdych pet minut na
// nekolik sekund objevila cerna obrazovka s "Nacitam animaci..." - nove zustane
// posledni snimek a pod indikatorem se objevi jen mala zluta poznamka. Kdyz
// stazeni selze, dosavadni snimky se nezahodi a overlay rekne, ze jsou starsi.
static void loadAndBuild() {
  const bool haveOld = (s_frameCount > 0);
  if (haveOld) {
    // Canvas jeste drzi posledni vykresleny snimek - staci pripsat poznamku.
    gfx->fillRect(LCD_WIDTH / 2 - 90, 32, 180, 12, C_BLACK);
    UI_TextCenteredIn(T(S_LOADING_NEWER), 0, LCD_WIDTH, 34, C_YELLOW, 1);
    gfx->flush();
  } else {
    gfx->fillScreen(C_BLACK);
    UI_TextCentered(LT("Nacitam animaci...", "Načítám animaci...", "Nacitavam animaciu...", "Načítavam animáciu..."), LCD_HEIGHT / 2, C_WHITE, 2);
    gfx->flush();
  }

  int n = CHMU_FetchAnim(ANIM_FRAMES);
  bool ok = (n > 0) && rebuildCrops();
  if (ok) {
    s_stale = false;
    s_fetchEvery = METEO_PERIOD_MS;
  } else if (haveOld && !s_wide && s_frameCount > 0) {
    // Dekodovane vyrezy jsou oddelene od stazenych PNG, takze je lze ukazovat
    // dal, i kdyz nove stazeni nedopadlo.
    s_stale = true;
    s_fetchEvery = METEO_RETRY_MS;
  } else {
    s_frameCount = 0;
    s_fetchEvery = METEO_RETRY_MS;
  }
  s_status = (s_frameCount > 0) ? "OK"
           : (s_wide ? T(S_FRAME_WIDE)
                     : LT("Chyba stahovani", "Chyba stahování", "Chyba stahovania", "Chyba sťahovania"));
}

// -----------------------------------------------------------------------------
static void drawOverlay() {
  const int cx = LCD_WIDTH / 2, cy = LCD_HEIGHT / 2;

  // Zamerny kriz na poloze uzivatele.
  gfx->drawFastHLine(cx - 8, cy, 16, C_WHITE);
  gfx->drawFastVLine(cx, cy - 8, 16, C_WHITE);

  // Legenda intenzity srazek - dBZ / mm/h (vlevo nahore).
  // Barvy prevzate ze SKUTECNE palety CHMU (reflektivitni blok, indexy 182-193),
  // takze sedi s mapou. Hodnoty mm/h prepocteny z dBZ (Marshall-Palmer).
  // Ctyri nizsi hodnoty opraveny v 0.8.5 podle barevne skaly CHMU
  // (scl-dbzmmh.png) overene na skutecnych snimcich.
  // Jde vypnout ve webovem nastaveni - kdo stupnici zna, uvidi vic uzemi.
  if (Settings_MeteoLegend()) {
    static const uint16_t COL[6] = { 0xA000, 0xF800, 0xFC20, 0xE6E0, 0x05E0, 0x001F };
    static const char*    LBL[6] = { ">56 / >100", "52 / 65", "44 / 21", "36 / 6.5", "24 / 1.2", "12 / 0.2" };
    const int lx = 4, ly = 4, boxW = 96, boxH = 12 + 6 * 13 + 2;
    gfx->fillRect(lx - 2, ly - 2, boxW, boxH, C_BLACK);   // podklad kvuli citelnosti
    gfx->setTextSize(1); gfx->setTextColor(C_GRAY);
    // Varianta "masked" (0.8.0) ma stejnou paletu, jen bledsi odstiny tam,
    // kde srazky nedopadaji na zem - nadpis rika, kterou se prave cte.
    if (CHMU_Masked()) { gfx->setTextColor(C_CYAN); gfx->setCursor(lx, ly); gfx->print("masked dBZ/mm/h"); }
    else               { gfx->setCursor(lx, ly); gfx->print("dBZ / mm/h"); }
    for (int i = 0; i < 6; i++) {
      int ry = ly + 12 + i * 13;
      gfx->fillRect(lx, ry, 9, 9, COL[i]);
      gfx->drawRect(lx, ry, 9, 9, C_DKGRAY);
      gfx->setTextColor(C_WHITE);
      gfx->setCursor(lx + 13, ry + 1); gfx->print(LBL[i]);
    }
  }

  // Indikator animace - horni stred: tecky + popisek casu ramce.
  {
    int n = s_frameCount > 0 ? s_frameCount : 1;
    int minAgo = (n - 1 - s_curFrame) * 5;
    char lbl[16];
    if (minAgo <= 0) snprintf(lbl, sizeof(lbl), "%s", T(S_NOW));
    else             snprintf(lbl, sizeof(lbl), "-%d min", minAgo);
    gfx->fillRect(cx - 52, 2, 104, 30, C_BLACK);   // podklad
    int gap = 16, totalW = (n - 1) * gap, sx0 = cx - totalW / 2, dy = 9;
    for (int i = 0; i < n; i++) {
      int x = sx0 + i * gap;
      if (i == s_curFrame) gfx->fillCircle(x, dy, 4, C_YELLOW);
      else                 gfx->drawCircle(x, dy, 4, C_GRAY);
    }
    UI_TextCenteredIn(lbl, 0, LCD_WIDTH, 20, C_CYAN, 1);
  }

  // Stazeni nevyslo a zobrazuje se predchozi sada - rict to, jinak by
  // hodinu stara animace vypadala jako aktualni.
  if (s_stale) {
    gfx->fillRect(LCD_WIDTH / 2 - 110, 32, 220, 12, C_BLACK);
    UI_TextCenteredIn(T(S_OLD_DATA), 0, LCD_WIDTH, 34, C_YELLOW, 1);
  }

  // Rozsah + indikator (vlevo dole) s podkladem.
  gfx->fillRect(2, LCD_HEIGHT - 26, 72, 24, C_BLACK);
  char rbuf[16];
  if (wholeCountry()) snprintf(rbuf, sizeof(rbuf), "%s", LT("cela CR", "celá ČR", "cela CR", "celá ČR"));
  else                snprintf(rbuf, sizeof(rbuf), "%.0f km", currentRange());
  gfx->setTextSize(1); gfx->setTextColor(C_YELLOW);
  gfx->setCursor(4, LCD_HEIGHT - 22); gfx->print(rbuf);
  int dotGap = 14, dotY = LCD_HEIGHT - 8, startX = 8;
  for (int i = 0; i < RANGE_COUNT; i++) {
    int x = startX + i * dotGap;
    if (i == s_rangeIdx) gfx->fillCircle(x, dotY, 3, C_YELLOW);
    else                 gfx->drawCircle(x, dotY, 3, C_GRAY);
  }
}

void ScreenWeather_Enter() {
  // Rozsah meteoradaru se pamatuje ZVLAST od rozsahu letadel - jsou to jine
  // veliciny (25-200 km vs. 10-100 km) a uzivatel je nastavuje nezavisle.
  int idx = Settings_MeteoRange();
  if (idx >= RANGE_COUNT) idx = 1;
  if (idx != s_rangeIdx) { s_rangeIdx = idx; s_needRebuild = true; }
  s_lastStep = millis();
  s_gap = false;
  s_curFrame = 0;
}

void ScreenWeather_ChangeRange(int dir) {
  s_rangeIdx = (s_rangeIdx + dir + RANGE_COUNT) % RANGE_COUNT;
  Settings_SetMeteoRange(s_rangeIdx);   // zapis do NVS je odlozeny
  s_needRebuild = true;   // ramce znovu orezat pri dalsim ticku
}

bool ScreenWeather_Tick() {
  if (WiFi.status() != WL_CONNECTED) { s_status = T(S_WIFI_WAIT); return false; }
  unsigned long now = millis();

  // Zmena rozsahu -> jen znovu orezat (ramce uz stazene).
  if (s_needRebuild && CHMU_AnimCount() > 0) {
    rebuildCrops();
    s_needRebuild = false; s_curFrame = 0; s_gap = false; s_lastStep = now;
    return true;
  }

  // Prvni nacteni - a po neuspechu dalsi pokus az za METEO_RETRY_MS, ne
  // v kazdem pruchodu smyckou.
  if (s_frameCount == 0) {
    if (s_lastFetch && now - s_lastFetch < s_fetchEvery) return false;
    loadAndBuild();
    s_lastFetch = now; s_curFrame = 0; s_gap = false; s_lastStep = now;
    return true;
  }

  // Periodicka obnova - jen kdyz jsme v pauze (zobrazuje se posledni snimek).
  if (s_gap && (now - s_lastFetch >= s_fetchEvery)) {
    loadAndBuild();
    s_lastFetch = now; s_curFrame = 0; s_gap = false; s_lastStep = now;
    return true;
  }

  // Animacni krok.
  if (s_gap) {
    if (now - s_gapStart >= 5000UL) { s_gap = false; s_curFrame = 0; s_lastStep = now; return true; }
    return false;
  }
  if (now - s_lastStep >= 500UL) {
    s_lastStep = now;
    s_curFrame++;
    if (s_curFrame >= s_frameCount) { s_curFrame = s_frameCount - 1; s_gap = true; s_gapStart = now; }
    return true;
  }
  return false;
}

void ScreenWeather_Draw() {
  gfx->fillScreen(C_BLACK);

  // Pevne prvky se rezervuji driv, nez EuBorder zacne narokovat misto pro
  // nazvy mest - popisek pod legendou nebo pod teckami by byl necitelny.
  // Souradnice odpovidaji PRESNE podkladovym obdelnikum, ktere kresli
  // drawOverlay() nize - kdyby se rozesly, Layout by hlidal jine misto, nez
  // se doopravdy kresli. (Prvni verze tady mela pruh pres celou sirku pro
  // indikator snimku a prilis velkou legendu; obojí hlasil test jako prekryv.)
  Layout_Begin();
  Layout_Reserve(LY_DOTS_X, LY_DOTS_Y0, LY_DOTS_W, LY_DOTS_H);  // tecky obrazovek
  Layout_Reserve(2, LCD_HEIGHT - 26, 72, 24);        // rozsah VCETNE svych tecek
  // Vypnuta legenda nesmi drzet svuj obdelnik - pixely by se sice uvolnily,
  // ale popisky mest by se tomu mistu dal vyhybaly a vypnuti by nic nepridalo.
  if (Settings_MeteoLegend()) Layout_Reserve(2, 2, 96, 92);   // legenda intenzity
  Layout_Reserve(LCD_WIDTH / 2 - 52, 2, 104, 30);    // indikator snimku (horni stred)
  if (s_stale) Layout_Reserve(LCD_WIDTH / 2 - 110, 32, 220, 12);  // poznamka o starych datech

  if (s_frameCount == 0) {
    UI_TextCentered(LT("Meteoradar CHMU", "Meteoradar ČHMÚ", "Meteoradar CHMU", "Meteoradar ČHMÚ"), LCD_HEIGHT / 2 - 20, C_WHITE, 2);
    UI_TextCentered(s_wide ? T(S_FRAME_WIDE) : s_status.c_str(), LCD_HEIGHT / 2 + 6, C_YELLOW, 2);
    return;
  }

  int f = clampI(s_curFrame, 0, s_frameCount - 1);
  s_crop565 = s_frame565[f];
  blitCrop();

  // Vyrez, ktery vubec muze byt videt - EuBorder podle nej zahodi zbytek
  // evropskych dat jeste pred sahnutim do framebufferu.
  {
    const float rng = viewRadiusKm();
    const double clat = viewLat(), clon = viewLon();
    const float marginKm = rng * 1.2f;
    const float dLat = marginKm / 111.0f;
    const float dLon = marginKm / (111.0f * cosf((float)clat * 0.0174532925f));
    const float lat0 = (float)clat - dLat, lat1 = (float)clat + dLat;
    const float lon0 = (float)clon - dLon, lon1 = (float)clon + dLon;

    EuBorder_Draw(borderProject, C_GRAY, lat0, lat1, lon0, lon1);

    int radius = (int)sqrtf((float)(LCD_WIDTH/2)*(LCD_WIDTH/2) +
                            (float)(LCD_HEIGHT/2)*(LCD_HEIGHT/2)) + 2;
    bool showFull = (rng <= 50.0f);
    uint8_t maxTier = (rng <= 100.0f) ? 3 : 2;
    EuBorder_DrawCities(borderProject, LCD_WIDTH / 2, LCD_HEIGHT / 2, radius,
                        C_WHITE, C_CYAN, showFull, maxTier,
                        lat0, lat1, lon0, lon1);
  }

  drawOverlay();
}

void ScreenWeather_SourceChanged() {
  s_frameCount = 0;
  s_lastFetch  = 0;
  s_stale      = false;
  s_fetchEvery = 0;
}
