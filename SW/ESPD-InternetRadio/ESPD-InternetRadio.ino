/*
 * LaskaKit ESPD-3.5" Internet Radio (ESP32-S3 + ILI9488 + FT5436)
 *
 * Board:   LaskaKit ESPD-3.5 ESP32 3.5 TFT ILI9488 Touch   https://www.laskakit.cz/laskakit-espd-35-esp32-3-5-tft-ili9488-touch/
 * Speaker: 3W 4Ω 40mm                                      https://www.laskakit.cz/reproduktor-3w-4-40mm/
 *
 * Funkce:
 *   - přehrávání internetových rádií (MP3/AAC/...), seznam stanic v Stations.h
 *   - 15pásmový spektrální analyzátor + stereo VU metr
 *   - běžící název skladby (diakritika se převádí na ASCII)
 *   - posuvník hlasitosti (tah prstem), ťuknutí na reproduktor = ztlumit
 *   - ťuknutí na kartu stanice = seznam stanic
 *   - ťuknutí na sluníčko = jas podsvícení (5 úrovní)
 *   - automatické znovupřipojení při výpadku Wi-Fi nebo streamu
 *   - stanice, hlasitost, jas a stav přehrávání se pamatují i po vypnutí
 *
 * How to steps:
 * 1. Copy file Setup303_ILI9488_ESPD-3_5_v3.h from https://github.com/LaskaKit/ESPD-35/tree/main/SW to Arduino/libraries/TFT_eSPI/User_Setups/
 * 2. in Arduino/libraries/TFT_eSPI/User_Setup_Select.h
 *      a. comment: #include <User_Setup.h>
 *      b. add: #include <User_Setups/Setup303_ILI9488_ESPD-3_5_v3.h>  // Setup file for LaskaKit ESPD-3.5" 320x480, ILI9488 V3
 * 3. Fill in WIFI_SSID / WIFI_PASSWORD below.
 *
 * Board settings:
 *          * ESP32S3 Dev Module
 *          * Flash Size: 16MB
 *          * Partition Scheme: "Huge APP"
 *          * PSRAM: OPI PSRAM   (nutné – sprity a audio buffery jsou v PSRAM)
 *
 * Libraries (tested):
 *          1. esp32 core: v3.3.10
 *          2. ESP32-audioI2S  https://github.com/schreibfaul1/ESP32-audioI2S  v4.0.0
 *          3. TFT_eSPI        https://github.com/Bodmer/TFT_eSPI              v2.5.43
 *
 * Email:podpora@laskakit.cz
 * Web:laskakit.cz
 */

#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include "Audio.h"

#include "FT6236.h"
#include "Orbitron_Medium_20.h"
#include "Theme.h"
#include "Stations.h"

// ====== Wi-Fi ======
const char* WIFI_SSID     = "xxxx";
const char* WIFI_PASSWORD = "xxxx";

// ====== Nastavení ======
constexpr uint8_t  VOLUME_STEPS        = 21;
constexpr uint8_t  BRIGHTNESS_LEVELS   = 5;
const uint8_t      BRIGHTNESS_PWM[BRIGHTNESS_LEVELS] = {20, 50, 100, 170, 255};
constexpr uint32_t WIFI_BOOT_WAIT_MS   = 20000;  // jak dlouho čekat na Wi-Fi při startu
constexpr uint32_t CONNECT_TIMEOUT_MS  = 12000;  // stream musí začít hrát do 12 s
constexpr uint32_t RETRY_MIN_MS        = 2000;   // znovupřipojení: 2 s, 4 s, 8 s ... max 30 s
constexpr uint32_t RETRY_MAX_MS        = 30000;
constexpr uint32_t PREFS_SAVE_DELAY_MS = 3000;   // šetříme flash – ukládáme až po chvíli klidu
constexpr uint32_t SPECTRUM_FRAME_MS   = 40;     // 25 FPS – víc oko nepozná, SPI ušetří
constexpr uint32_t MARQUEE_STEP_MS     = 28;
constexpr uint32_t MARQUEE_PAUSE_MS    = 1500;
constexpr int      MARQUEE_GAP_PX      = 48;
constexpr int      NUM_BANDS           = 15;     // počet pásem, které posílá ESP32-audioI2S

// ====== Hardware ======
FT6236      ts(480, 320);
TFT_eSPI    tft;
TFT_eSprite sprIcons(&tft), sprCard(&tft), sprTitle(&tft), sprSpec(&tft), sprVol(&tft), sprBtns(&tft);
Audio       audio;
Preferences prefs;

// ====== Stav ======
enum class Play : uint8_t { Stopped, Connecting, Playing, Error };

struct RadioState {
  Play     play       = Play::Stopped;
  bool     wantPlay   = false;   // uživatel chce hrát → při výpadku se znovu připojíme
  uint8_t  station    = 0;
  uint8_t  volume     = 12;
  bool     muted      = false;
  uint8_t  brightness = 2;
  String   title;                // název skladby (už převedený na ASCII)
  String   error;
  uint32_t bitrate    = 0;       // b/s
  uint32_t sampleRate = 0;
  String   codec;
} R;

uint32_t connectStartMs = 0;
uint32_t retryAtMs      = 0;
uint32_t retryDelayMs   = RETRY_MIN_MS;
uint32_t prefsSaveAtMs  = 0;

// Wi-Fi + baterie (pro hlavičku)
bool    wifiConnected = false;
uint8_t wifiBars      = 0;       // 0..3
int     batteryPct    = -1;      // -1 = neměřeno
bool    batteryCharging = false;

// Spektrum / VU (plní audio callback)
struct Meters {
  uint8_t  bars[NUM_BANDS]  = {0};
  uint8_t  peaks[NUM_BANDS] = {0};
  uint8_t  vu[4]            = {0};   // L, R, peak L, peak R  (0..255)
  uint32_t lastEventMs      = 0;
  bool     dirty            = true;
} M;
uint16_t segColors[SPEC_SEGS];

// ====== Audio task ======
// audio.loop() (čtení streamu ze sítě) běží ve vlastním tasku s vyšší prioritou než
// kreslení – SPI přenosy na displej ani blokující connecttohost() (TLS handshake trvá
// i několik sekund) tak nezdržují stream ani dotyk. Funkce knihovny, které sahají
// na stream (connecttohost, stopSong, loop), volá výhradně tento task.
enum class AudioCmd : uint8_t { Play, Stop };
struct AudioMsg { AudioCmd cmd; uint8_t station; uint32_t seq; };

QueueHandle_t     audioQueue;                  // délka 1 + xQueueOverwrite → platí jen poslední příkaz
SemaphoreHandle_t audioMutex;                  // chrání AudioShared
volatile uint32_t connectSeq     = 0;          // poslední požadavek z UI
volatile uint32_t connectDoneSeq = 0;          // poslední dokončený v audio tasku
volatile bool     connectOk      = false;

// data z audio callbacku → UI si je vyzvedne v pullAudioEvents()
struct AudioShared {
  String   title;
  bool     titleNew  = false;
  uint32_t bitrate   = 0;
  uint8_t  bars[NUM_BANDS]  = {0};
  uint8_t  peaks[NUM_BANDS] = {0};
  uint8_t  vu[4]            = {0};
  uint32_t meterMs   = 0;
  bool     metersNew = false;
} A;

// Běžící text
int      mqOffset = 0;
int      mqTextW  = 0;
uint32_t mqNextMs = 0;

// Co je potřeba překreslit
enum : uint8_t { D_ICONS = 1, D_CARD = 2, D_TITLE = 4, D_VOLUME = 8, D_BUTTONS = 16, D_ALL = 0x1F };
uint8_t dirty = D_ALL;

// UI režim + dotyk
enum class Screen : uint8_t { Main, List };
Screen screen   = Screen::Main;
int    listPage = 0;

enum Target : int {
  T_NONE = 0, T_PREV, T_PLAY, T_NEXT, T_VOLUME, T_MUTE, T_BRIGHT, T_CARD,
  T_LIST_CLOSE, T_LIST_PGUP, T_LIST_PGDN, T_LIST_ROW /* + index řádku */
};
int     pressed     = T_NONE;
bool    touchDown   = false;
uint8_t touchMisses = 0;

// ============================================================================
//  Pomocné funkce
// ============================================================================

// UTF-8 → ASCII (čeština, slovenština, němčina, polština ...). Vestavěné fonty
// diakritiku neumí, místo "kostiček" tak zobrazíme aspoň písmeno bez háčku.
String asciiFold(const char* s) {
  static const char LATIN1[]  = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";        // U+00C0..U+00FF
  static const char LATIN_A[] = "AaAaAaCcCcCcCcDdDdEeEeEeEeEeGgGgGgGgHhHhIiIiIiIiIiJjJjKkkLlLlLlLlLlNnNnNnnNnOoOoOoOoRrRrRrSsSsSsSsTtTtTtUuUuUuUuUuUuWwYyYZzZzZzs"; // U+0100..U+017F
  String out;
  if (!s) return out;
  out.reserve(strlen(s));
  const uint8_t* p = (const uint8_t*)s;
  while (*p) {
    uint32_t cp;
    if (*p < 0x80)                                   { cp = *p++; }
    else if ((*p & 0xE0) == 0xC0 && p[1])            { cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F); p += 2; }
    else if ((*p & 0xF0) == 0xE0 && p[1] && p[2])    { cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); p += 3; }
    else                                             { p++; continue; }

    if (cp >= 0x20 && cp < 0x7F)          out += (char)cp;
    else if (cp >= 0xC0 && cp <= 0xFF)    out += LATIN1[cp - 0xC0];
    else if (cp >= 0x100 && cp <= 0x17F)  out += LATIN_A[cp - 0x100];
    else if (cp == 0x2018 || cp == 0x2019) out += '\'';
    else if (cp == 0x201C || cp == 0x201D) out += '"';
    else if (cp == 0x2013 || cp == 0x2014) out += '-';
    else if (cp == 0x2026)                out += "...";
    else if (cp == 0xA0)                  out += ' ';
  }
  out.trim();
  return out;
}

static inline bool inRect(int px, int py, int x, int y, int w, int h) {
  return px >= x && px < x + w && py >= y && py < y + h;
}
static inline bool inCircle(int px, int py, int cx, int cy, int r) {
  return (px - cx) * (px - cx) + (py - cy) * (py - cy) <= r * r;
}

// Zkrátí text tak, aby se vešel do maxW (přidá "...")
String fitText(TFT_eSPI& g, String s, int maxW) {
  if (g.textWidth(s) <= maxW) return s;
  while (s.length() && g.textWidth(s + "...") > maxW) s.remove(s.length() - 1);
  s.trim();
  return s + "...";
}

void savePrefsLater() { prefsSaveAtMs = millis() + PREFS_SAVE_DELAY_MS; }

void savePrefsNow() {
  prefs.putUChar("st",   R.station);
  prefs.putUChar("vol",  R.volume);
  prefs.putUChar("bl",   R.brightness);
  prefs.putBool ("mute", R.muted);
  prefs.putBool ("play", R.wantPlay);
  prefsSaveAtMs = 0;
  Serial.println("[prefs] saved");
}

void loadPrefs() {
  prefs.begin("radio", false);
  R.station    = prefs.getUChar("st", 0);
  R.volume     = prefs.getUChar("vol", 12);
  R.brightness = prefs.getUChar("bl", 2);
  R.muted      = prefs.getBool ("mute", false);
  R.wantPlay   = prefs.getBool ("play", true);   // po prvním zapnutí rovnou hraj
  if (R.station >= NUM_STATIONS)          R.station = 0;
  if (R.volume > VOLUME_STEPS)            R.volume = 12;
  if (R.brightness >= BRIGHTNESS_LEVELS)  R.brightness = 2;
}

// ============================================================================
//  Vykreslování – ikony
// ============================================================================

void drawSun(TFT_eSprite& g, int cx, int cy, uint8_t level) {
  const int rays = 4 + 2 * level;                 // 4..12 paprsků podle jasu
  g.fillSmoothCircle(cx, cy, 5, COL_AMBER, COL_BG);
  for (int i = 0; i < rays; i++) {
    float a = i * TWO_PI / rays - HALF_PI;
    g.drawWideLine(cx + 8 * cosf(a), cy + 8 * sinf(a), cx + 12 * cosf(a), cy + 12 * sinf(a), 2, COL_AMBER, COL_BG);
  }
}

void drawWifi(TFT_eSprite& g, int cx, int baseY) {
  // drawSmoothArc: 0° = dole, po směru hodin → 180° je nahoře
  for (int i = 0; i < 3; i++) {
    int r = 7 + i * 6;
    uint16_t c = (wifiConnected && wifiBars > i) ? COL_TEAL : COL_CARD_HI;
    g.drawSmoothArc(cx, baseY, r, r - 3, 135, 225, c, COL_BG, true);
  }
  g.fillSmoothCircle(cx, baseY - 1, 2, wifiConnected ? COL_TEAL : COL_CARD_HI, COL_BG);
  if (!wifiConnected) {                           // červený křížek
    g.drawWideLine(cx + 7, baseY - 9, cx + 13, baseY - 3, 2, COL_ERR, COL_BG);
    g.drawWideLine(cx + 13, baseY - 9, cx + 7, baseY - 3, 2, COL_ERR, COL_BG);
  }
}

void drawBattery(TFT_eSprite& g, int x, int cy) {
  const int w = 28, h = 14, y = cy - h / 2;
  g.fillSmoothRoundRect(x, y, w, h, 3, COL_MUTED, COL_BG);
  g.fillSmoothRoundRect(x + 2, y + 2, w - 4, h - 4, 2, COL_BG, COL_MUTED);
  g.fillRoundRect(x + w, cy - 3, 3, 6, 1, COL_MUTED);
  if (batteryPct < 0) return;

  uint16_t c = batteryPct > 40 ? COL_TEAL : batteryPct > 15 ? COL_AMBER : COL_ERR;
  if (batteryCharging) c = COL_TEAL;
  int fw = max(2, (w - 8) * batteryPct / 100);
  g.fillRect(x + 4, y + 4, fw, h - 8, c);
  if (batteryCharging) {                          // blesk
    int bx = x + w / 2;
    g.fillTriangle(bx + 2, y - 1, bx - 4, cy + 1, bx, cy + 1, COL_TEXT);
    g.fillTriangle(bx - 2, y + h + 1, bx + 4, cy - 1, bx, cy - 1, COL_TEXT);
  }
}

void drawSpeaker(TFT_eSprite& g, int x, int cy, bool muted, uint8_t vol, uint16_t bg) {
  uint16_t c = muted ? COL_MUTED : COL_TEAL_LIGHT;
  g.fillRect(x, cy - 4, 5, 8, c);
  g.fillTriangle(x + 4, cy, x + 11, cy - 8, x + 11, cy + 8, c);
  g.fillRect(x + 4, cy - 4, 2, 8, c);
  if (muted) {
    g.drawWideLine(x + 15, cy - 5, x + 23, cy + 5, 2, COL_ERR, bg);
    g.drawWideLine(x + 23, cy - 5, x + 15, cy + 5, 2, COL_ERR, bg);
    return;
  }
  // vlnky podle hlasitosti (0° = dole, po směru hodin → 270° je vpravo)
  if (vol > 0)                g.drawSmoothArc(x + 10, cy, 7, 5, 225, 315, c, bg, true);
  if (vol > VOLUME_STEPS / 2) g.drawSmoothArc(x + 10, cy, 12, 10, 225, 315, c, bg, true);
}

void drawChevrons(TFT_eSprite& g, int cx, int cy, bool right, uint16_t c) {
  const int h = 18, w = 12, gap = 1;
  int x0 = cx - w - gap / 2;
  for (int i = 0; i < 2; i++) {
    int x = x0 + i * (w + gap);
    if (right) g.fillTriangle(x, cy - h / 2, x, cy + h / 2, x + w, cy, c);
    else       g.fillTriangle(x + w, cy - h / 2, x + w, cy + h / 2, x, cy, c);
  }
}

// ============================================================================
//  Vykreslování – části obrazovky
// ============================================================================

void drawHeaderStatic() {
  tft.fillRect(0, 0, SCR_W, HDR_H, COL_BG);
  tft.setFreeFont(&Orbitron_Medium_20);
  tft.setTextColor(COL_TEXT);
  tft.setTextDatum(L_BASELINE);
  tft.drawString("Internet Radio", 14, 29);
  // dvoubarevná linka v barvách LaskaKit
  tft.fillRect(0, HDR_H, SCR_W * 2 / 3, 2, COL_TEAL);
  tft.fillRect(SCR_W * 2 / 3, HDR_H, SCR_W - SCR_W * 2 / 3, 2, COL_ORANGE);
}

void renderIcons() {
  sprIcons.fillSprite(COL_BG);
  drawSun(sprIcons, HDR_SUN_CX, 22, R.brightness);
  drawWifi(sprIcons, HDR_WIFI_CX, 31);
  drawBattery(sprIcons, HDR_BAT_X, 22);
  sprIcons.pushSprite(HDR_ICONS_X, 0);
}

void statusPill(const char*& text, uint16_t& color) {
  if (!wifiConnected && R.wantPlay)        { text = "NO WI-FI";   color = COL_ERR;   return; }
  switch (R.play) {
    case Play::Playing:    text = "LIVE";       color = COL_TEAL;  break;
    case Play::Connecting: text = "CONNECTING"; color = COL_AMBER; break;
    case Play::Error:      text = retryAtMs ? "RETRYING" : "ERROR"; color = COL_ERR; break;
    default:               text = "STOPPED";    color = COL_MUTED; break;
  }
}

void drawChip(TFT_eSprite& g, int& x, int y, const String& s) {
  int w = g.textWidth(s, 2) + 16;
  g.fillSmoothRoundRect(x, y, w, 22, 11, COL_CARD_HI, COL_CARD);
  g.setTextColor(COL_TEAL_LIGHT, COL_CARD_HI);
  g.setTextDatum(MC_DATUM);
  g.drawString(s, x + w / 2, y + 11, 2);
  x += w + 6;
}

void renderTitle();

void renderCard() {
  TFT_eSprite& g = sprCard;
  g.fillSprite(COL_BG);
  g.fillSmoothRoundRect(0, 0, CARD_W, CARD_H, 14, COL_CARD, COL_BG);

  // 1. řádek: číslo stanice + stav
  g.setTextFont(2);
  g.setTextDatum(ML_DATUM);
  g.setTextColor(COL_MUTED, COL_CARD);
  char buf[24];
  snprintf(buf, sizeof(buf), "STATION %d / %d", R.station + 1, NUM_STATIONS);
  g.drawString(buf, CARD_PAD, 22);

  const char* st; uint16_t sc;
  statusPill(st, sc);
  int pw = g.textWidth(st, 2) + 30;
  int px = CARD_W - CARD_PAD - pw;
  g.fillSmoothRoundRect(px, 11, pw, 22, 11, COL_BG, COL_CARD);
  g.fillSmoothCircle(px + 12, 22, 4, sc, COL_BG);
  g.setTextColor(sc, COL_BG);
  g.drawString(st, px + 21, 22, 2);

  // název stanice – velký font, když se nevejde, menší
  String name = asciiFold(STATIONS[R.station].name);
  const int maxW = CARD_W - 2 * CARD_PAD;
  g.setTextColor(COL_TEXT);
  g.setTextDatum(L_BASELINE);
  g.setFreeFont(&FreeSansBold18pt7b);
  if (g.textWidth(name) > maxW) g.setFreeFont(&FreeSansBold12pt7b);
  g.drawString(fitText(g, name, maxW), CARD_PAD, 68);

  // tyrkysovo-oranžový akcent pod názvem
  g.fillRect(CARD_PAD, 76, 24, 3, COL_TEAL);
  g.fillRect(CARD_PAD + 24, 76, 10, 3, COL_ORANGE);

  // oddělovač + "čipy" s informacemi o streamu
  g.drawFastHLine(CARD_PAD, 118, CARD_W - 2 * CARD_PAD, COL_CARD_HI);
  int cx = CARD_PAD;
  if (R.bitrate) {
    uint32_t kbps = R.bitrate >= 1000 ? (R.bitrate + 500) / 1000 : R.bitrate;
    drawChip(g, cx, 130, String(kbps) + " kbps");
  }
  if (R.codec.length())  drawChip(g, cx, 130, R.codec);
  if (R.sampleRate)      drawChip(g, cx, 130, String(R.sampleRate / 1000.0f, 1) + " kHz");
  if (cx == CARD_PAD) {
    g.setTextColor(COL_MUTED, COL_CARD);
    g.setTextDatum(ML_DATUM);
    g.drawString(wifiConnected ? WiFi.localIP().toString() : String("Wi-Fi: ") + WIFI_SSID, CARD_PAD, 141, 2);
  }

  // ikona seznamu (ťukni na kartu)
  for (int i = 0; i < 3; i++)
    g.fillSmoothRoundRect(CARD_W - CARD_PAD - 18, 133 + i * 6, 18, 3, 1, COL_TEAL, COL_CARD);

  g.pushSprite(CARD_X, CARD_Y);
  renderTitle();
}

void resetMarquee() {
  mqOffset = 0;
  mqNextMs = millis() + MARQUEE_PAUSE_MS;
}

void renderTitle() {
  TFT_eSprite& g = sprTitle;
  g.fillSprite(COL_CARD);
  g.setFreeFont(&FreeSans9pt7b);
  g.setTextDatum(L_BASELINE);

  String s; uint16_t c = COL_TEAL_LIGHT;
  if (R.error.length())                { s = R.error;  c = COL_ERR; }
  else if (R.title.length())           { s = R.title; }
  else if (R.play == Play::Connecting) { s = "Connecting...";       c = COL_MUTED; }
  else if (R.play == Play::Playing)    { s = "Live stream";         c = COL_MUTED; }
  else                                 { s = "Press play to start"; c = COL_MUTED; }

  g.setTextColor(c);
  mqTextW = g.textWidth(s);
  if (mqTextW <= TITLE_W) {
    g.drawString(s, 0, 17);
  } else {
    g.drawString(s, -mqOffset, 17);
    g.drawString(s, -mqOffset + mqTextW + MARQUEE_GAP_PX, 17);
    // jemné zeslabení okrajů
    for (int i = 0; i < 10; i++) {
      uint8_t a = 255 - i * 25;
      for (int y = 0; y < TITLE_H; y++) {
        g.drawPixel(i, y, tft.alphaBlend(a, COL_CARD, g.readPixel(i, y)));
        g.drawPixel(TITLE_W - 1 - i, y, tft.alphaBlend(a, COL_CARD, g.readPixel(TITLE_W - 1 - i, y)));
      }
    }
  }
  g.pushSprite(TITLE_X, TITLE_Y);
}

void renderVolume() {
  TFT_eSprite& g = sprVol;
  g.fillSprite(COL_BG);
  drawSpeaker(g, 2, VOL_H / 2, R.muted, R.volume, COL_BG);

  const int cy = VOL_H / 2;
  const int x0 = VOL_TRACK_X0, x1 = VOL_TRACK_X1;
  int kx = x0 + (x1 - x0) * R.volume / VOLUME_STEPS;
  uint16_t fill = R.muted ? COL_MUTED : COL_TEAL;
  g.fillSmoothRoundRect(x0, cy - 3, x1 - x0, 6, 3, COL_CARD_HI, COL_BG);
  if (kx > x0) g.fillSmoothRoundRect(x0, cy - 3, kx - x0, 6, 3, fill, COL_BG);
  g.fillSmoothCircle(kx, cy, 10, COL_TEXT, COL_BG);
  g.fillSmoothCircle(kx, cy, 4, fill, COL_TEXT);

  g.setTextColor(R.muted ? COL_MUTED : COL_TEXT, COL_BG);
  g.setTextDatum(MR_DATUM);
  g.drawString(String(R.volume), VOL_W, cy + 1, 4);
  g.pushSprite(VOL_X, VOL_Y);
}

void renderButtons() {
  TFT_eSprite& g = sprBtns;
  g.fillSprite(COL_BG);
  const int cy = BTN_CY;

  g.fillSmoothCircle(BTN_PREV_CX, cy, BTN_SIDE_R, pressed == T_PREV ? COL_CARD_HI : COL_CARD, COL_BG);
  drawChevrons(g, BTN_PREV_CX, cy, false, COL_TEAL_LIGHT);
  g.fillSmoothCircle(BTN_NEXT_CX, cy, BTN_SIDE_R, pressed == T_NEXT ? COL_CARD_HI : COL_CARD, COL_BG);
  drawChevrons(g, BTN_NEXT_CX, cy, true, COL_TEAL_LIGHT);

  // hlavní tlačítko – oranžová "akční" barva LaskaKit
  const int pcx = BTN_PLAY_CX;
  g.fillSmoothCircle(pcx, cy, BTN_PLAY_R, pressed == T_PLAY ? COL_AMBER : COL_ORANGE, COL_BG);
  if (R.play == Play::Playing) {                       // pauza
    g.fillSmoothRoundRect(pcx - 11, cy - 13, 8, 26, 2, COL_DARK, COL_ORANGE);
    g.fillSmoothRoundRect(pcx + 3,  cy - 13, 8, 26, 2, COL_DARK, COL_ORANGE);
  } else if (R.wantPlay) {                             // připojuje se → stop
    g.fillSmoothRoundRect(pcx - 10, cy - 10, 20, 20, 3, COL_DARK, COL_ORANGE);
  } else {                                             // play (opticky posunutý doprava)
    g.fillTriangle(pcx - 8, cy - 15, pcx - 8, cy + 15, pcx + 15, cy, COL_DARK);
  }

  // točící se oblouk kolem tlačítka během připojování
  if (R.wantPlay && R.play != Play::Playing) {
    uint32_t a = (millis() / 4) % 360;
    g.drawSmoothArc(pcx, cy, BTN_PLAY_R + 5, BTN_PLAY_R + 2, a, (a + 90) % 360, COL_AMBER, COL_BG, true);
  }
  g.pushSprite(0, BTN_Y);
}

void initSpectrumColors() {
  // spodek tyrkysový → přes žlutou → nahoře oranžový
  for (int s = 0; s < SPEC_SEGS; s++) {
    float t = float(s) / (SPEC_SEGS - 1);
    uint16_t c;
    if (t < 0.5f)       c = COL_TEAL;
    else if (t < 0.8f)  c = tft.alphaBlend(uint8_t((t - 0.5f) / 0.3f * 255), COL_AMBER, COL_TEAL);
    else                c = tft.alphaBlend(uint8_t((t - 0.8f) / 0.2f * 255), COL_ORANGE, COL_AMBER);
    segColors[s] = c;
  }
}

void renderSpectrum() {
  TFT_eSprite& g = sprSpec;
  g.fillSprite(COL_BG);

  const int barW = (SPEC_W - (NUM_BANDS - 1) * SPEC_GAP) / NUM_BANDS;   // 16 px
  const int segPitch = SPEC_BARS_H / SPEC_SEGS;                          // 5 px
  const int segH = segPitch - 2;

  for (int b = 0; b < NUM_BANDS; b++) {
    int x = b * (barW + SPEC_GAP);
    int lit  = (M.bars[b]  * SPEC_SEGS + 127) / 255;
    int peak = (M.peaks[b] * SPEC_SEGS + 127) / 255;
    for (int s = 0; s < SPEC_SEGS; s++) {
      int y = SPEC_BARS_H - (s + 1) * segPitch;
      uint16_t c = s < lit ? segColors[s] : COL_GRID;
      if (s == peak - 1 && peak > lit) c = COL_TEAL_LIGHT;
      g.fillRect(x, y, barW, segH, c);
    }
  }

  // stereo VU pod spektrem
  g.setTextFont(1);
  g.setTextDatum(ML_DATUM);
  for (int ch = 0; ch < 2; ch++) {
    int y = SPEC_BARS_H + 8 + ch * 8;
    g.setTextColor(COL_MUTED, COL_BG);
    g.drawString(ch ? "R" : "L", 0, y + 2);
    const int x0 = 10, w = SPEC_W - x0;
    g.fillRoundRect(x0, y, w, 5, 2, COL_GRID);
    int fw = w * M.vu[ch] / 255;
    if (fw > 1) g.fillRoundRect(x0, y, fw, 5, 2, COL_TEAL);
    int pk = x0 + w * M.vu[2 + ch] / 255;
    if (M.vu[2 + ch] > 4) g.fillRect(min(pk, x0 + w - 3), y, 3, 5, COL_ORANGE);
  }
  g.pushSprite(SPEC_X, SPEC_Y);
}

void drawMainScreen() {
  tft.fillScreen(COL_BG);
  drawHeaderStatic();
  M.dirty = true;
  dirty = D_ALL;
}

// ============================================================================
//  Seznam stanic
// ============================================================================

int listPages() { return (NUM_STATIONS + LIST_ROWS - 1) / LIST_ROWS; }

void drawListScreen() {
  tft.fillRect(0, HDR_H + 2, SCR_W, SCR_H - HDR_H - 2, COL_BG);

  tft.setTextDatum(ML_DATUM);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString("Stations", MARGIN + 4, LIST_Y + 22);
  // zavírací křížek
  tft.fillSmoothCircle(SCR_W - MARGIN - 20, LIST_Y + 22, 18, COL_CARD, COL_BG);
  tft.drawWideLine(SCR_W - MARGIN - 26, LIST_Y + 16, SCR_W - MARGIN - 14, LIST_Y + 28, 3, COL_TEAL_LIGHT, COL_CARD);
  tft.drawWideLine(SCR_W - MARGIN - 14, LIST_Y + 16, SCR_W - MARGIN - 26, LIST_Y + 28, 3, COL_TEAL_LIGHT, COL_CARD);

  for (int i = 0; i < LIST_ROWS; i++) {
    int idx = listPage * LIST_ROWS + i;
    if (idx >= NUM_STATIONS) break;
    int y = LIST_ROW_Y + i * LIST_ROW_H;
    bool cur = idx == R.station;
    tft.fillSmoothRoundRect(MARGIN, y, CARD_W, LIST_ROW_H - 6, 10, cur ? COL_TEAL_DIM : COL_CARD, COL_BG);
    if (cur) tft.fillSmoothRoundRect(MARGIN, y + 8, 4, LIST_ROW_H - 22, 2, COL_TEAL, COL_TEAL_DIM);

    uint16_t rowBg = cur ? COL_TEAL_DIM : COL_CARD;
    tft.setTextColor(cur ? COL_TEAL : COL_MUTED, rowBg);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(String(idx + 1), MARGIN + 24, y + (LIST_ROW_H - 6) / 2, 2);

    tft.setFreeFont(cur ? &FreeSansBold9pt7b : &FreeSans9pt7b);
    tft.setTextColor(cur ? COL_TEXT : COL_TEAL_LIGHT, rowBg);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(fitText(tft, asciiFold(STATIONS[idx].name), CARD_W - 80), MARGIN + 44, y + (LIST_ROW_H - 6) / 2);

    if (cur && R.play == Play::Playing) {          // malý "ekvalizér" u hrající stanice
      for (int k = 0; k < 3; k++) {
        int h = 8 + (k % 2) * 6;
        tft.fillRect(MARGIN + CARD_W - 30 + k * 6, y + 27 - h, 4, h, COL_ORANGE);
      }
    }
  }

  if (listPages() > 1) {
    const int cy = LIST_NAV_Y + 24;
    tft.fillSmoothCircle(MARGIN + 30, cy, 20, listPage > 0 ? COL_CARD : COL_BG, COL_BG);
    tft.fillSmoothCircle(SCR_W - MARGIN - 30, cy, 20, listPage < listPages() - 1 ? COL_CARD : COL_BG, COL_BG);
    if (listPage > 0)
      tft.fillTriangle(MARGIN + 34, cy - 8, MARGIN + 34, cy + 8, MARGIN + 24, cy, COL_TEAL_LIGHT);
    if (listPage < listPages() - 1)
      tft.fillTriangle(SCR_W - MARGIN - 34, cy - 8, SCR_W - MARGIN - 34, cy + 8, SCR_W - MARGIN - 24, cy, COL_TEAL_LIGHT);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_MUTED, COL_BG);
    tft.drawString(String(listPage + 1) + " / " + String(listPages()), SCR_W / 2, cy, 2);
  }
}

void openList() {
  screen = Screen::List;
  listPage = R.station / LIST_ROWS;
  drawListScreen();
}

void closeList() {
  screen = Screen::Main;
  drawMainScreen();
}

// ============================================================================
//  Přehrávání
// ============================================================================

void setPlay(Play p, const char* err = nullptr) {
  R.play  = p;
  R.error = err ? err : "";
  resetMarquee();
  dirty |= D_CARD | D_BUTTONS;
}

void clearStreamInfo() {
  R.title = "";
  R.codec = "";
  R.bitrate = 0;
  R.sampleRate = 0;
}

void scheduleRetry(const char* why);

void startPlayback() {
  R.wantPlay = true;
  retryAtMs = 0;
  clearStreamInfo();
  savePrefsLater();

  if (WiFi.status() != WL_CONNECTED) {             // spustí se samo po připojení Wi-Fi
    setPlay(Play::Error, "Waiting for Wi-Fi...");
    return;
  }

  setPlay(Play::Connecting);
  connectStartMs = millis();
  Serial.printf("[radio] connecting to %s\n", STATIONS[R.station].url);
  AudioMsg m{AudioCmd::Play, R.station, connectSeq + 1};
  connectSeq = m.seq;
  xQueueOverwrite(audioQueue, &m);
}

void sendStop() {
  AudioMsg m{AudioCmd::Stop, 0, 0};
  xQueueOverwrite(audioQueue, &m);
}

void stopPlayback() {
  R.wantPlay = false;
  retryAtMs = 0;
  retryDelayMs = RETRY_MIN_MS;
  sendStop();
  clearStreamInfo();
  setPlay(Play::Stopped);
  savePrefsLater();
}

void selectStation(int idx) {
  idx = (idx + NUM_STATIONS) % NUM_STATIONS;
  R.station = idx;
  retryDelayMs = RETRY_MIN_MS;
  startPlayback();
}

void scheduleRetry(const char* why) {
  setPlay(Play::Error, why);
  if (!R.wantPlay) return;
  retryAtMs = millis() + retryDelayMs;
  Serial.printf("[radio] %s - retry in %lu s\n", why, (unsigned long)(retryDelayMs / 1000));
  retryDelayMs = min(retryDelayMs * 2, RETRY_MAX_MS);
}

void setVolume(int v) {
  v = constrain(v, 0, VOLUME_STEPS);
  if (v == R.volume && !R.muted) return;
  R.volume = v;
  R.muted = false;
  audio.setMute(false);
  audio.setVolume(R.volume);
  dirty |= D_VOLUME;
  savePrefsLater();
}

void toggleMute() {
  R.muted = !R.muted;
  audio.setMute(R.muted);
  dirty |= D_VOLUME;
  savePrefsLater();
}

void setBrightness(uint8_t level) {
  R.brightness = level % BRIGHTNESS_LEVELS;
  analogWrite(TFT_BL, BRIGHTNESS_PWM[R.brightness]);
  dirty |= D_ICONS;
  savePrefsLater();
}

void updatePlayback() {
  const uint32_t now = millis();

  switch (R.play) {
    case Play::Connecting:
      if (now - connectStartMs > CONNECT_TIMEOUT_MS) {
        sendStop();
        scheduleRetry("Connection timeout");
      } else if (connectDoneSeq == connectSeq) {   // audio task už connecttohost() dokončil
        if (!connectOk) {
          scheduleRetry("Cannot connect to the stream");
        } else if (!audio.isRunning()) {
          scheduleRetry("Stream not available");
        } else if (audio.getBitRate() > 0 || audio.getSampleRate() > 0) {
          Serial.println("[radio] playing");
          retryDelayMs = RETRY_MIN_MS;
          setPlay(Play::Playing);
        }
      }
      break;

    case Play::Playing:
      if (!audio.isRunning()) scheduleRetry("Stream lost");
      break;

    default:
      if (retryAtMs && (int32_t)(now - retryAtMs) >= 0) {
        retryAtMs = 0;
        if (R.wantPlay) startPlayback();
      }
      break;
  }

  // informace o streamu (kodek, vzorkování) – stačí kontrolovat občas
  static uint32_t lastInfo = 0;
  if (R.play == Play::Playing && now - lastInfo > 500) {
    lastInfo = now;
    String   codec = audio.getCodec() ? audio.getCodecname() : "";
    uint32_t sr    = audio.getSampleRate();
    uint32_t br    = audio.getBitRate();
    if (codec != R.codec || sr != R.sampleRate || (br && !R.bitrate)) {
      R.codec = codec;
      R.sampleRate = sr;
      if (br && !R.bitrate) R.bitrate = br;
      dirty |= D_CARD;
    }
  }
}

// Callback z ESP32-audioI2S – volá se z audio.loop(), tedy z audio tasku.
// Jen uložíme data pod mutexem, kreslí se v hlavní smyčce.
void onAudioEvent(Audio::msg_t m) {
  switch (m.e) {
    case Audio::evt_streamtitle: {
      String t = asciiFold(m.msg);
      xSemaphoreTake(audioMutex, portMAX_DELAY);
      A.title = t;
      A.titleNew = true;
      xSemaphoreGive(audioMutex);
      Serial.printf("[title] %s\n", m.msg ? m.msg : "");
      break;
    }
    case Audio::evt_bitrate:
      if (m.arg1 > 0) A.bitrate = m.arg1;
      break;
    case Audio::evt_vu:
    case Audio::evt_spectrum:
      xSemaphoreTake(audioMutex, portMAX_DELAY);
      if (m.e == Audio::evt_vu) {
        for (size_t i = 0; i < 4 && i < m.vec1.size(); i++) A.vu[i] = m.vec1[i];
      } else {
        for (size_t i = 0; i < NUM_BANDS && i < m.vec1.size(); i++) A.bars[i]  = m.vec1[i];
        for (size_t i = 0; i < NUM_BANDS && i < m.vec2.size(); i++) A.peaks[i] = m.vec2[i];
      }
      A.meterMs = millis();
      A.metersNew = true;
      xSemaphoreGive(audioMutex);
      break;
    default:
      Serial.printf("[%s] %s\n", m.s ? m.s : "audio", m.msg ? m.msg : "");
      break;
  }
}

// Převezme data z audio tasku do stavu UI
void pullAudioEvents() {
  xSemaphoreTake(audioMutex, portMAX_DELAY);
  if (A.titleNew) {
    A.titleNew = false;
    if (A.title != R.title) {
      R.title = A.title;
      resetMarquee();
      dirty |= D_TITLE;
    }
  }
  if (A.metersNew) {
    A.metersNew = false;
    memcpy(M.bars,  A.bars,  sizeof(M.bars));
    memcpy(M.peaks, A.peaks, sizeof(M.peaks));
    memcpy(M.vu,    A.vu,    sizeof(M.vu));
    M.lastEventMs = A.meterMs;
    M.dirty = true;
  }
  xSemaphoreGive(audioMutex);

  uint32_t br = A.bitrate;
  if (br && br != R.bitrate && R.play == Play::Playing) {
    R.bitrate = br;
    dirty |= D_CARD;
  }
}

void audioTask(void*) {
  AudioMsg m;
  for (;;) {
    if (xQueueReceive(audioQueue, &m, 0) == pdTRUE) {
      audio.stopSong();
      if (m.cmd == AudioCmd::Play) {
        A.bitrate = 0;
        bool ok = audio.connecttohost(STATIONS[m.station].url);
        connectOk = ok;
        connectDoneSeq = m.seq;
      }
    }
    audio.loop();
    vTaskDelay(1);
  }
}

// ============================================================================
//  Wi-Fi, baterie, ukládání
// ============================================================================

void updateWiFi() {
  static uint32_t last = 0, lostAt = 0, lastReconnect = 0;
  const uint32_t now = millis();
  if (now - last < 1000) return;
  last = now;

  bool conn = WiFi.status() == WL_CONNECTED;
  uint8_t bars = 0;
  if (conn) {
    int rssi = WiFi.RSSI();
    bars = rssi > -60 ? 3 : rssi > -70 ? 2 : rssi > -80 ? 1 : 0;
  }
  if (bars != wifiBars) { wifiBars = bars; dirty |= D_ICONS; }

  if (conn != wifiConnected) {
    wifiConnected = conn;
    dirty |= D_ICONS | D_CARD;
    Serial.printf("[wifi] %s\n", conn ? WiFi.localIP().toString().c_str() : "disconnected");
    if (conn) {
      if (R.wantPlay && R.play != Play::Playing) retryAtMs = now + 500;
    } else {
      lostAt = now;
    }
  }

  // knihovna se obvykle připojí sama, ale pojistíme to
  if (!conn && now - lostAt > 10000 && now - lastReconnect > 15000) {
    lastReconnect = now;
    Serial.println("[wifi] reconnect");
    WiFi.reconnect();
  }
}

void updateBattery(bool force = false) {
  static uint32_t last = 0;
  if (!force && millis() - last < 5000) return;
  last = millis();

  uint32_t mv = 0;
  for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(BAT_PIN);
  float v = mv / 8.0f * deviderRatio / 1000.0f;

  bool charging = v > 4.25f;                       // napájení z USB / nabíjení
  int  pct = constrain(int((v - 3.3f) / (4.15f - 3.3f) * 100.0f + 0.5f), 0, 100);
  if (v < 2.5f) pct = -1;                          // baterie není připojená
  if (pct != batteryPct || charging != batteryCharging) {
    batteryPct = pct;
    batteryCharging = charging;
    dirty |= D_ICONS;
  }
}

void updatePrefs() {
  if (prefsSaveAtMs && (int32_t)(millis() - prefsSaveAtMs) >= 0) savePrefsNow();
}

// ============================================================================
//  Dotyk
// ============================================================================

int hitTest(int x, int y) {
  if (screen == Screen::List) {
    if (inCircle(x, y, SCR_W - MARGIN - 20, LIST_Y + 22, 28)) return T_LIST_CLOSE;
    if (listPages() > 1 && y >= LIST_NAV_Y) {
      if (x < SCR_W / 3)      return T_LIST_PGUP;
      if (x > SCR_W * 2 / 3)  return T_LIST_PGDN;
    }
    for (int i = 0; i < LIST_ROWS; i++) {
      int idx = listPage * LIST_ROWS + i;
      if (idx < NUM_STATIONS && inRect(x, y, MARGIN, LIST_ROW_Y + i * LIST_ROW_H, CARD_W, LIST_ROW_H - 6))
        return T_LIST_ROW + idx;
    }
    return T_NONE;
  }

  if (y < HDR_H + 6 && x >= HDR_ICONS_X && x < HDR_ICONS_X + 40) return T_BRIGHT;
  if (inRect(x, y, CARD_X, CARD_Y, CARD_W, CARD_H))              return T_CARD;
  if (inRect(x, y, VOL_X, VOL_Y - 6, VOL_W, VOL_H + 8))
    return x < VOL_X + VOL_TRACK_X0 - 8 ? T_MUTE : T_VOLUME;
  const int by = BTN_Y + BTN_CY;
  if (inCircle(x, y, BTN_PLAY_CX, by, BTN_PLAY_R + 10)) return T_PLAY;
  if (inCircle(x, y, BTN_PREV_CX, by, BTN_SIDE_R + 14)) return T_PREV;
  if (inCircle(x, y, BTN_NEXT_CX, by, BTN_SIDE_R + 14)) return T_NEXT;
  return T_NONE;
}

void volumeFromX(int x) {
  const int x0 = VOL_X + VOL_TRACK_X0, x1 = VOL_X + VOL_TRACK_X1;
  setVolume(((constrain(x, x0, x1) - x0) * VOLUME_STEPS + (x1 - x0) / 2) / (x1 - x0));
}

void onPress(int x, int y) {
  int t = hitTest(x, y);
  Serial.printf("[touch] %d,%d -> %d\n", x, y, t);
  pressed = t;

  // akce hned při stisku – rádio reaguje okamžitě, puštění jen zruší zvýraznění
  switch (t) {
    case T_VOLUME: volumeFromX(x); break;
    case T_PLAY:   R.wantPlay ? stopPlayback() : startPlayback(); break;
    case T_PREV:   selectStation(R.station - 1); break;
    case T_NEXT:   selectStation(R.station + 1); break;
    case T_MUTE:   toggleMute(); break;
    case T_BRIGHT: setBrightness(R.brightness + 1); break;
    case T_CARD:   openList(); break;
    case T_LIST_CLOSE: closeList(); break;
    case T_LIST_PGUP:  if (listPage > 0)               { listPage--; drawListScreen(); } break;
    case T_LIST_PGDN:  if (listPage < listPages() - 1) { listPage++; drawListScreen(); } break;
    default:
      if (t >= T_LIST_ROW) {
        int idx = t - T_LIST_ROW;
        screen = Screen::Main;
        drawMainScreen();
        if (idx != R.station || !R.wantPlay) selectStation(idx);
      }
      break;
  }
  dirty |= D_BUTTONS;
}
void onDrag(int x, int y) {
  if (pressed == T_VOLUME) volumeFromX(x);
}

void onRelease() {
  if (pressed == T_PREV || pressed == T_PLAY || pressed == T_NEXT) dirty |= D_BUTTONS;
  pressed = T_NONE;
}
void handleTouch() {
  static uint32_t last = 0;
  if (millis() - last < 10) return;
  last = millis();

  TS_Point p;
  if (ts.read(p)) {                                // jedna I2C transakce: počet doteků + souřadnice
    touchMisses = 0;
    if (!touchDown) { touchDown = true; onPress(p.x, p.y); }
    else            onDrag(p.x, p.y);
  } else if (touchDown && ++touchMisses >= 3) {    // 3 prázdná čtení (~30 ms) = puštěno, odfiltruje zákmity
    touchDown = false;
    onRelease();
  }
}
// ============================================================================
//  Animace a překreslování
// ============================================================================

void updateMeters() {
  static uint32_t lastFrame = 0;
  const uint32_t now = millis();
  if (now - lastFrame < SPECTRUM_FRAME_MS) return;
  lastFrame = now;

  // když nechodí data (stop / buffering), sloupce plynule spadnou
  if (now - M.lastEventMs > 150) {
    bool any = false;
    for (int i = 0; i < NUM_BANDS; i++) {
      M.bars[i]  = M.bars[i]  * 4 / 5;
      M.peaks[i] = M.peaks[i] > 6 ? M.peaks[i] - 6 : 0;
      any |= M.bars[i] | M.peaks[i];
    }
    for (int i = 0; i < 4; i++) { M.vu[i] = M.vu[i] * 4 / 5; any |= M.vu[i]; }
    if (any) M.dirty = true;
  }

  if (M.dirty) {
    M.dirty = false;
    renderSpectrum();
  }
}

void updateMarquee() {
  if (mqTextW <= TITLE_W || (int32_t)(millis() - mqNextMs) < 0) return;
  mqOffset++;
  if (mqOffset >= mqTextW + MARQUEE_GAP_PX) {
    mqOffset = 0;
    mqNextMs = millis() + MARQUEE_PAUSE_MS;
  } else {
    mqNextMs = millis() + MARQUEE_STEP_MS;
  }
  dirty |= D_TITLE;
}

void renderDirty() {
  if (screen != Screen::Main) { dirty = 0; return; }
  if (dirty & D_ICONS)   renderIcons();
  if (dirty & D_CARD)    renderCard();                 // vykreslí i titulek
  else if (dirty & D_TITLE) renderTitle();
  if (dirty & D_VOLUME)  renderVolume();
  if (dirty & D_BUTTONS) renderButtons();
  dirty = 0;
}

// ============================================================================
//  Start
// ============================================================================

bool createSprites() {
  bool ok = true;
  ok &= sprIcons.createSprite(HDR_ICONS_W, HDR_H) != nullptr;
  ok &= sprCard .createSprite(CARD_W, CARD_H)     != nullptr;
  ok &= sprTitle.createSprite(TITLE_W, TITLE_H)   != nullptr;
  ok &= sprSpec .createSprite(SPEC_W, SPEC_H)     != nullptr;
  ok &= sprVol  .createSprite(VOL_W, VOL_H)       != nullptr;
  ok &= sprBtns .createSprite(SCR_W, BTN_H)       != nullptr;
  return ok;
}

void splash(const String& status, uint16_t color, uint32_t spinAngle) {
  const int cy = SCR_H / 2 - 20;
  tft.fillRect(0, cy + 70, SCR_W, 30, COL_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(color, COL_BG);
  tft.drawString(status, SCR_W / 2, cy + 84, 2);
  tft.drawSmoothArc(SCR_W / 2, cy, 34, 30, 0, 360, COL_CARD, COL_BG, false);
  tft.drawSmoothArc(SCR_W / 2, cy, 34, 30, spinAngle % 360, (spinAngle + 80) % 360, COL_ORANGE, COL_CARD, true);
}

void connectWiFiAtBoot() {
  tft.fillScreen(COL_BG);
  const int cy = SCR_H / 2 - 20;
  tft.setFreeFont(&Orbitron_Medium_20);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COL_TEAL);
  tft.drawString("LaskaKit", SCR_W / 2, cy - 90);
  tft.setTextColor(COL_MUTED);
  tft.drawString("ESPD-3.5 Radio", SCR_W / 2, cy - 62);
  tft.fillRect(SCR_W / 2 - 40, cy - 46, 54, 2, COL_TEAL);
  tft.fillRect(SCR_W / 2 + 14, cy - 46, 26, 2, COL_ORANGE);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);                            // stabilnější stream
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_BOOT_WAIT_MS) {
    splash(String("Connecting to ") + WIFI_SSID + " ...", COL_TEAL_LIGHT, (millis() - t0) / 3);
    delay(20);
  }

  if (WiFi.status() == WL_CONNECTED) {
    splash("Connected  " + WiFi.localIP().toString(), COL_TEAL, 0);
    delay(600);
  } else {
    splash("Wi-Fi not available - retrying in background", COL_ERR, 0);
    delay(1200);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, 0);                          // bez probliknutí smetí po startu

  loadPrefs();

  tft.init();
  tft.setRotation(0);
  tft.setTextWrap(false, false);
  tft.fillScreen(COL_BG);
  analogWrite(TFT_BL, BRIGHTNESS_PWM[R.brightness]);

  if (!createSprites()) Serial.println("!! Sprite allocation failed – enable OPI PSRAM in Tools menu");
  initSpectrumColors();

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  if (!ts.begin(40)) Serial.println("FT6236 init failed");
  ts.setRotation(2);

  Audio::audio_info_callback = onAudioEvent;
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolumeSteps(VOLUME_STEPS);
  audio.setVolume(R.volume);
  audio.setMute(R.muted);
  audio.settings.VU_LEVEL = true;
  audio.settings.SPECTRUM = true;

  audioMutex = xSemaphoreCreateMutex();
  audioQueue = xQueueCreate(1, sizeof(AudioMsg));
  // jádro 1 (stejné jako loop), ale vyšší priorita → stream má vždy přednost před kreslením;
  // dekodér knihovny běží na jádře 0
  xTaskCreatePinnedToCore(audioTask, "audioNet", 16384, nullptr, 2, nullptr, 1);

  connectWiFiAtBoot();
  wifiConnected = WiFi.status() == WL_CONNECTED;
  updateBattery(true);

  drawMainScreen();
  renderDirty();
  renderSpectrum();

  if (R.wantPlay) startPlayback();
}

void loop() {
  pullAudioEvents();
  handleTouch();
  updatePlayback();
  updateWiFi();
  updateBattery();
  updatePrefs();

  if (screen == Screen::Main) {
    updateMeters();
    updateMarquee();
    static uint32_t lastSpin = 0;                  // animace oblouku při připojování
    if (R.wantPlay && R.play != Play::Playing && millis() - lastSpin > 60) {
      lastSpin = millis();
      dirty |= D_BUTTONS;
    }
  }
  renderDirty();
  vTaskDelay(1);
}
