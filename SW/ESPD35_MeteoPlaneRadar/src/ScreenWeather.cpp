// =============================================================================
//  ESPD35_MeteoPlaneRadar - obrazovka meteoradaru CHMU s ANIMACI (6 ramcu).
//  Port z petus/Meteo-PlaneRadar na obdelnik 480x320.
//
//  Souradnice: Web Mercator, vyrez s pomerem stran 1.5:1 (izotropni meritko).
//  Animace: 6 ramcu (-25 .. nyni), 2 snimky/s, mezi cykly ~5 s pauza.
//  Nove ramce se stahuji jen kdyz se zobrazuje posledni snimek (v pauze),
//  aby se animace nepresekavala. Behem stahovani "Nacitam animaci...".
// =============================================================================
#include "ScreenWeather.h"
#include "CHMU.h"
#include "Settings.h"
#include "UI.h"
#include "Config.h"
#include "CzBorder.h"

#include <WiFi.h>
#include <PNGdec.h>
#include <math.h>
#include <esp_heap_caps.h>

#define ANIM_FRAMES  CHMU_ANIM_MAX   // 6

// Rozsahy meteoradaru (polomer na VYSKU v km).
static const float RANGES_KM[] = {25.0f, 50.0f, 100.0f, 200.0f};
static const int   RANGE_COUNT = sizeof(RANGES_KM) / sizeof(RANGES_KM[0]);
static int s_rangeIdx = 1;
static float currentRange() { return RANGES_KM[s_rangeIdx]; }

static PNG png;
static int s_imgW = 600, s_imgH = 480;
static int s_dataX1 = 100000, s_dataY0 = -1;   // hranice DAT (mimo = titulek/skala)
static String s_status = "Start...";
static bool s_wide = false;              // snimek moc siroky pro PNGdec

struct Crop { int x1, y1, x2, y2; };
static Crop s_crop;
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
  s_crop.x1 = clampI(x1, 0, s_imgW - 1);
  s_crop.x2 = clampI(x2, 0, s_imgW - 1);
  s_crop.y1 = clampI(y1, 0, s_imgH - 1);
  s_crop.y2 = clampI(y2, 0, s_imgH - 1);
}
static int cropW() { return s_crop.x2 - s_crop.x1 + 1; }
static int cropH() { return s_crop.y2 - s_crop.y1 + 1; }

static void borderProject(float lat, float lon, int* sx, int* sy) {
  int srcX = lonToX(lon), srcY = latToY(lat);
  *sx = (int)((int64_t)(srcX - s_crop.x1) * LCD_WIDTH  / cropW());
  *sy = (int)((int64_t)(srcY - s_crop.y1) * LCD_HEIGHT / cropH());
}

static int pngDraw(PNGDRAW* d) {
  int srcY = d->y;
  if (srcY < s_crop.y1 || srcY > s_crop.y2) return 1;
  if (!s_crop565 || !s_lineBuf) return 1;
  png.getLineAsRGB565(d, s_lineBuf, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
  const int cw = cropW();
  uint16_t* row = s_crop565 + (int64_t)(srcY - s_crop.y1) * cw;
  for (int i = 0; i < cw; i++) {
    int srcX = clampI(s_crop.x1 + i, 0, s_imgW - 1);
    // Mimo oblast dat (titulek nahore, skala vpravo v PNG) = cerna.
    row[i] = (srcX > s_dataX1 || srcY < s_dataY0) ? 0x0000 : s_lineBuf[srcX];
  }
  return 1;
}

// Roztahne aktualni vyrez (s_crop565) pres celou obrazovku (nearest neighbour).
static void blitCrop() {
  if (!s_crop565) return;
  const int cw = cropW(), ch = cropH();
  for (int dy = 0; dy < LCD_HEIGHT; dy++) {
    int srcRow = clampI((int)((int64_t)dy * ch / LCD_HEIGHT), 0, ch - 1);
    const uint16_t* row = s_crop565 + (int64_t)srcRow * cw;
    for (int dx = 0; dx < LCD_WIDTH; dx++) {
      int srcCol = clampI((int)((int64_t)dx * cw / LCD_WIDTH), 0, cw - 1);
      gfx->drawPixel(dx, dy, row[srcCol]);
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

  makeCrop(Settings_Lat(), Settings_Lon(), currentRange());
  int need = cropW() * cropH();

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

// Stazeni ramcu + sestaveni (blokujici). Behem toho hlaska na displeji.
static void loadAndBuild() {
  gfx->fillScreen(C_BLACK);
  UI_TextCentered("Nacitam animaci...", LCD_HEIGHT / 2, C_WHITE, 2);
  gfx->flush();
  int n = CHMU_FetchAnim(ANIM_FRAMES);
  if (n > 0) rebuildCrops(); else s_frameCount = 0;
  s_status = (s_frameCount > 0) ? "OK" : (s_wide ? "snimek moc siroky" : "Chyba stahovani");
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
  {
    static const uint16_t COL[6] = { 0xA000, 0xF800, 0xFC20, 0xE6E0, 0x05E0, 0x001F };
    static const char*    LBL[6] = { ">56 / >100", "52 / 65", "46 / 27", "40 / 12", "32 / 3.6", "20 / <1" };
    const int lx = 4, ly = 4, boxW = 96, boxH = 12 + 6 * 13 + 2;
    gfx->fillRect(lx - 2, ly - 2, boxW, boxH, C_BLACK);   // podklad kvuli citelnosti
    gfx->setTextSize(1); gfx->setTextColor(C_GRAY);
    gfx->setCursor(lx, ly); gfx->print("dBZ / mm/h");
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
    if (minAgo <= 0) snprintf(lbl, sizeof(lbl), "nyni");
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

  // Rozsah + indikator (vlevo dole) s podkladem.
  gfx->fillRect(2, LCD_HEIGHT - 26, 64, 24, C_BLACK);
  char rbuf[16];
  snprintf(rbuf, sizeof(rbuf), "%.0f km", currentRange());
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
  s_lastStep = millis();
  s_gap = false;
  s_curFrame = 0;
}

void ScreenWeather_ChangeRange(int dir) {
  s_rangeIdx = (s_rangeIdx + dir + RANGE_COUNT) % RANGE_COUNT;
  s_needRebuild = true;   // ramce znovu orezat pri dalsim ticku
}

bool ScreenWeather_Tick() {
  if (WiFi.status() != WL_CONNECTED) { s_status = "Ceka na WiFi"; return false; }
  unsigned long now = millis();

  // Zmena rozsahu -> jen znovu orezat (ramce uz stazene).
  if (s_needRebuild && CHMU_AnimCount() > 0) {
    rebuildCrops();
    s_needRebuild = false; s_curFrame = 0; s_gap = false; s_lastStep = now;
    return true;
  }

  // Prvni nacteni.
  if (s_frameCount == 0) {
    loadAndBuild();
    s_lastFetch = now; s_curFrame = 0; s_gap = false; s_lastStep = now;
    return true;
  }

  // Periodicka obnova - jen kdyz jsme v pauze (zobrazuje se posledni snimek).
  if (s_gap && (now - s_lastFetch >= 300000UL)) {
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

  if (s_frameCount == 0) {
    UI_TextCentered("Meteoradar CHMU", LCD_HEIGHT / 2 - 20, C_WHITE, 2);
    UI_TextCentered(s_wide ? "snimek moc siroky" : s_status.c_str(), LCD_HEIGHT / 2 + 6, C_YELLOW, 2);
    return;
  }

  int f = clampI(s_curFrame, 0, s_frameCount - 1);
  s_crop565 = s_frame565[f];
  blitCrop();

  CzBorder_Draw(borderProject, C_GRAY);
  {
    int radius = (int)sqrtf((float)(LCD_WIDTH/2)*(LCD_WIDTH/2) + (float)(LCD_HEIGHT/2)*(LCD_HEIGHT/2)) + 2;
    float rng = currentRange();
    bool showFull  = (rng <= 50.0f);
    bool showSmall = (rng <= 100.0f);
    CzBorder_DrawCities(borderProject, LCD_WIDTH / 2, LCD_HEIGHT / 2, radius,
                        C_WHITE, C_CYAN, showFull, showSmall);
  }

  drawOverlay();
}
