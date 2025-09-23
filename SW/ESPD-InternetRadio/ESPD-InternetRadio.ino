/*
  LaskaKit ESPD-3.5" Internet Radio (ESP32-S3 + ILI9488 + FT5436)
  Audio:   schreibfaul1/ESP32-audioI2S
  Displej: TFT_eSPI (User_Setup pro ESPD-3.5)
  Touch:   FT6236/FT5436 (lokálně v projektu)

  Změna: VU meter je jeden analogový sloupec z audio.getVUlevel()
*/

#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <FS.h>
#include <TFT_eSPI.h>
#include "Audio.h"

#include "FT6236.h"
#include "Orbitron_Medium_20.h"

// ====== Wi-Fi ======
const char* SSID     = "xxxxx";
const char* PASSWORD = "xxxxx";

// ====== Displej / barvy ======
#define TFT_RES_X 320
#define TFT_RES_Y 480
#define CENTRE_X  (TFT_RES_X/2)

// ====== Displej / barvy (Teal Dark Theme) ======
#define COL_BG        0x0863
#define COL_TEXT      TFT_WHITE

#define COL_BTN_PREV  0x2945
#define COL_BTN_PLAY  0x39E7
#define COL_BTN_NEXT  0x2945
#define COL_ICON      TFT_WHITE   // navy/royal blue-ish; klidně dolaď

#define COL_BAR_BG    0x2945    // tmavý pruh
#define COL_BAR_FG    0x07F9    // tyrkys

#define COL_LINE      0x04EF    // světle azurová linka
#define COL_ERR       TFT_RED

// VU meter
#define COL_VU        0x07E0
#define COL_VU_PEAK   0xFFE0

const int PADDING = 10;
const int TEXTPADDING = 8;

// spodek UI
const int BTN_H   = 72;
const int BTN_W   = 100;
const int BTN_Y   = TFT_RES_Y - PADDING - BTN_H;
const int VOL_H   = 24;
const int VOL_Y   = BTN_Y - VOL_H - 12;
const int NAME_Y  = VOL_Y - 26;

// horní pruhy
const int HEADER_H     = 34;
const int BRIGHT_H     = 30;
const int STATUS_H     = 28;

const int BRIGHT_Y     = HEADER_H + 16;
const int STATUS_Y     = BRIGHT_Y + BRIGHT_H + 2;

// VU blok – jeden sloupec
const int VU_W       = TFT_RES_X - 60;
const int VU_H       = 120;
const int VU_X       = (TFT_RES_X - VU_W) / 2;
const int VU_Y       = NAME_Y - 150;

// --- song / stream info (font 2 + marquee) ---
String   gSongTitle = "";     // název skladby
String   gCodec     = "";     // typ dat / kodek (např. MP3/AAC)
String   gBitrate   = "";     // např. "128 kbps"

// souřadnice řádků (oba font 2)
// původně:  TITLE_Y = NAME_Y + 20;  INFO_Y = TITLE_Y + 16;
const int TITLE_Y = BRIGHT_Y + 6 + 5*18;   // název skladby nad názvem stanice
const int INFO_Y  = BRIGHT_Y + 6 + 6*18;  // bitrate + typ těsně pod title (pořád nad NAME_Y)

// Marquee (posun dlouhého názvu)
int      titleScrollX   = 0;
int      titleTextW     = 0;
uint32_t titleLastMs    = 0;
const    uint16_t TITLE_AREA_W = TFT_RES_X - 2*TEXTPADDING;
const    uint16_t TITLE_SPEED_MS = 25; // menší = rychlejší
const    uint16_t TITLE_GAP_PX   = 36; // mezera mezi konci textu při loopu

// Jas (5 kroků)
int backlightSteps[5] = {15, 50, 110, 180, 240};
uint8_t backlightIdx = 2;

// Touch + displej + audio
FT6236   ts(480, 320);
TFT_eSPI tft;
Audio    audio;

// Baterie (fallback)
#ifndef BAT_PIN
#define BAT_PIN A0
#endif
#ifndef deviderRatio
#define deviderRatio 1.769
#endif

// ====== Stanice ======
struct Station { const char* name; const char* url; };
Station stations[] = {
  {"Rock Radio",  "https://playerservices.streamtheworld.com/api/livestream-redirect/ROCK_RADIO_128.mp3"},
  {"Radio Beat",  "https://icecast2.play.cz/radiobeat128.mp3"},
  {"Rock Zone",   "https://icecast2.play.cz/rockzone128.mp3"},
  {"Frekvence 1", "https://ice.actve.net/fm-frekvence1-aac"}
};
const int NUM_ST = sizeof(stations)/sizeof(Station);
int stIndex = 0;

// ====== Stav ======
enum PlayStatus { ST_STOPPED, ST_PLAYING, ST_ERROR };
PlayStatus gStatus = ST_STOPPED;
String     gErrMsg = "";
String     gMeta   = "";

bool     waitingConnect = false;
uint32_t connStartMs    = 0;
const   uint32_t CONNECT_TIMEOUT_MS = 8000;  // 8 s

// ============ VU meter (stereo, 7 svislých segmentů) ============
// Každý kanál (L/R) zabere polovinu výšky VU, segmenty jsou přes celou výšku řádku.
static const uint8_t  VU_SEGMENTS = 8;
static const uint16_t VU_COL_INACTIVE = 0x0443;

static const uint16_t VU_COLS[VU_SEGMENTS] = {
  0x07E0, 0x07E0, 0x07E0,   // green
  0xFFE0, 0xFFE0,           // yellow
  0xFD20,                   // orange
  0xF800                    // red
};

static const int VU_SEG_GAP = 8;   // mezera mezi „cihlami“

uint8_t segL_last = 255, segR_last = 255;
uint32_t vu_last_ms = 0;
const uint32_t VU_FRAME_MS = 33;   // ~30 FPS

// ---------- Utility ----------
static inline bool hit(int px, int py, int x, int y, int w, int h) {
  return (px >= x && px <= x+w && py >= y && py <= y+h);
}

// ============== vykreslování ==============
void drawHeader() {
  tft.fillScreen(COL_BG);
  tft.setSwapBytes(true);

  tft.setFreeFont(&Orbitron_Medium_20);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Internet Radio", TEXTPADDING, 6);
  tft.drawLine(0, HEADER_H, TFT_RES_X, HEADER_H, COL_LINE);
}

// indikátor jasu – slunce s paprsky (vpravo)
void drawBrightnessSun() {

  int cx = TFT_RES_X - 34;
  int cy = BRIGHT_Y + BRIGHT_H/2;
  int r  = 9;
  uint16_t sunFill = TFT_YELLOW;
  uint16_t sunRay  = TFT_ORANGE;

  // clear brightness number
  tft.fillRect(TEXTPADDING + tft.textWidth("Brightness: ", 2), cy - 23, tft.textWidth("10", 2), tft.fontHeight(2), COL_BG);
  // clear the sun
  tft.fillRect(cx - 23, cy - 23, 46, 46, COL_BG);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextFont(2);
  tft.drawString("Brightness: " + String(backlightIdx + 1), TEXTPADDING, BRIGHT_Y + 6);

  tft.fillCircle(cx, cy, r, sunFill);

  int raysByLevel[5] = {4, 6, 8, 10, 12};
  int rays = raysByLevel[backlightIdx];
  float step = TWO_PI / float(rays);
  int rayLen = 14;

  for (int i = 0; i < rays; i++) {
    float a = i * step;
    int x1 = cx + int((r + 2) * cos(a));
    int y1 = cy + int((r + 2) * sin(a));
    int x2 = cx + int((r + rayLen) * cos(a));
    int y2 = cy + int((r + rayLen) * sin(a));
    tft.drawLine(x1, y1, x2, y2, sunRay);
  }
}

void drawStatusLine() {
  tft.fillRect(0, BRIGHT_Y + 6 + 4*18 - 2, TFT_RES_X, 12, COL_BG);

  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);
  tft.setTextColor((gStatus==ST_ERROR) ? COL_ERR : COL_TEXT, COL_BG);

  String s = (gStatus==ST_PLAYING) ? "Status: Playing"
           : (gStatus==ST_ERROR)   ? "Status: Error"
                                   : "Status: Stopped";
  tft.drawString(s, TEXTPADDING, BRIGHT_Y + 6 + 4*18);

  String r = (gStatus==ST_ERROR) ? ("Err: " + gErrMsg) : gMeta;
  if (r.length()) {
    while (r.length() && tft.textWidth(r) > (TFT_RES_X - 20)) r.remove(r.length()-1);
    tft.setTextDatum(TR_DATUM);
    tft.drawString(r, TFT_RES_X-6, BRIGHT_Y + 6 + 4*18);
  }

  drawBrightnessSun();
}

void drawVolumeBar(uint8_t vol) { // 0..21
  const int x = PADDING;
  const int w = TFT_RES_X - 2*PADDING;
  tft.fillRoundRect(x, VOL_Y, w, VOL_H, 6, COL_BAR_BG);
  int fill = map(vol, 0, 21, 0, w);
  tft.fillRoundRect(x, VOL_Y, fill, VOL_H, 6, COL_BAR_FG);

  tft.fillRect(TEXTPADDING + tft.textWidth("Volume: ", 2), BRIGHT_Y + 6 + 18, tft.textWidth("99", 2), tft.fontHeight(2), COL_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextFont(2);
  tft.drawString("Volume: " + String(vol), TEXTPADDING, BRIGHT_Y + 6 + 18);
}

void drawStationName(const String& s) {
  tft.fillRect(0, NAME_Y-2, TFT_RES_X, 24, COL_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextFont(4);
  tft.drawString(s, TEXTPADDING, NAME_Y);
}

void drawBattery() {
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextDatum(TR_DATUM);
  tft.setTextFont(2);
  tft.setTextPadding(tft.textWidth("9.99 V"));
  float v = analogReadMilliVolts(BAT_PIN) * (deviderRatio / 1000.0f);
  tft.drawString(String(v, 2) + " V", TFT_RES_X - 6, 10);
}

// --- tlačítka (grafické ikony) ---
// --- dvojitá šipka dovnitř tlačítka ---
// kreslí dvě plné trojúhelníkové "chevrony" podle obrázku
// --------- Ikona: dvojitý šíp (chevron) zarovnaný na střed tlačítka ---------
static void drawDoubleChevron(int x, int y, int w, int h, uint16_t color, bool toRight)
{
  // velikost ikony s příjemným vnitřním okrajem
  const float scale = 0.5f;                         // celková výška ikony vůči tlačítku
  const int   iconH = int(h * scale);
  const int   triW  = int(iconH * 0.48f);            // šířka jednoho trojúhelníku
  const int   gap   = max(2, int(iconH * 0.08f));    // mezera mezi šipkami
  const int   iconW = triW * 2 + gap;

  const int   top   = y + (h - iconH) / 2;
  const int   left  = x + (w - iconW) / 2;

  if (toRight) {
    // první (levější) trojúhelník ►
    tft.fillTriangle(left,            top,
                     left,            top + iconH,
                     left + triW,     top + iconH / 2, color);
    // druhý trojúhelník ►
    int l2 = left + triW + gap;
    tft.fillTriangle(l2,              top,
                     l2,              top + iconH,
                     l2 + triW,       top + iconH / 2, color);
  } else {
    // první (pravější) trojúhelník ◄
    int r1 = left + iconW;
    tft.fillTriangle(r1,              top,
                     r1,              top + iconH,
                     r1 - triW,       top + iconH / 2, color);
    // druhý trojúhelník ◄
    int r2 = r1 - triW - gap;
    tft.fillTriangle(r2,              top,
                     r2,              top + iconH,
                     r2 - triW,       top + iconH / 2, color);
  }
}

// --- TLAČÍTKA: použijeme výše, ikona je teď vždy přesně uprostřed ---
void drawButtonPrev(int x, int y, int w, int h) {
  tft.fillRoundRect(x, y, w, h, 10, COL_BTN_PREV);
  drawDoubleChevron(x, y, w, h, TFT_WHITE, /*toRight=*/false);
}

void drawButtonPlayPause(int x, int y, int w, int h, bool isPlaying) {
  tft.fillRoundRect(x, y, w, h, 10, COL_BTN_PLAY);
  int cx = x + w/2, cy = y + h/2, sz = h/3;
  if (!isPlaying) {
    tft.fillTriangle(cx - sz/2, cy - sz,
                     cx - sz/2, cy + sz,
                     cx + sz,   cy, TFT_WHITE);
  } else {
    int barW = max(6, sz/3), barH = 2*sz;
    tft.fillRect(cx - barW - 6, cy - barH/2, barW, barH, TFT_WHITE);
    tft.fillRect(cx + 6,        cy - barH/2, barW, barH, TFT_WHITE);
  }
}

void drawButtonNext(int x, int y, int w, int h) {
  tft.fillRoundRect(x, y, w, h, 10, COL_BTN_NEXT);
  drawDoubleChevron(x, y, w, h, TFT_WHITE, /*toRight=*/true);
}



void drawButtons() {
  int gap = (TFT_RES_X - 3*BTN_W) / 4;
  int x1 = gap;
  int x2 = x1 + BTN_W + gap;
  int x3 = x2 + BTN_W + gap;

  drawButtonPrev(x1, BTN_Y, BTN_W, BTN_H);
  drawButtonPlayPause(x2, BTN_Y, BTN_W, BTN_H, (gStatus==ST_PLAYING || waitingConnect));
  drawButtonNext(x3, BTN_Y, BTN_W, BTN_H);
}

void drawVUBackground() {
  // rám a vymazání
  tft.fillRect(VU_X-4, VU_Y-4, VU_W+8, VU_H+8, COL_BG);
  tft.drawRoundRect(VU_X-10, VU_Y-10, VU_W+20, VU_H+20, 6, COL_LINE);

  // popisky L/R vlevo
  tft.setTextDatum(ML_DATUM);
  tft.drawString("L", VU_X - 14, VU_Y + (VU_H/4));
  tft.drawString("R", VU_X - 14, VU_Y + (3*VU_H/4));

  // základní šedé „cihly“
  int rowH = (VU_H - VU_SEG_GAP) / 2;                    // výška řádku (L nebo R)
  int segW = (VU_W - (VU_SEGMENTS-1)*VU_SEG_GAP) / VU_SEGMENTS;

  for (int row = 0; row < 2; row++) {
    int baseY = VU_Y + row * (rowH + VU_SEG_GAP);
    for (int s = 0; s < VU_SEGMENTS; s++) {
      int x = VU_X + s * (segW + VU_SEG_GAP);
      tft.fillRoundRect(x, baseY, segW, rowH, 6, VU_COL_INACTIVE);
    }
  }

  segL_last = segR_last = 255; // vynutit první překreslení
}

static inline void drawVURow(uint8_t lit, int baseY, int rowH) {
  int segW = (VU_W - (VU_SEGMENTS-1)*VU_SEG_GAP) / VU_SEGMENTS;
  for (int s = 0; s < VU_SEGMENTS; s++) {
    int x = VU_X + s * (segW + VU_SEG_GAP);
    uint16_t col = (s < lit) ? VU_COLS[s] : VU_COL_INACTIVE;
    tft.fillRoundRect(x, baseY, segW, rowH, 6, col);
  }
}

void updateVU() {
  if (millis() - vu_last_ms < VU_FRAME_MS) return;
  vu_last_ms = millis();

  // z knihovny: high byte = L (0..127), low byte = R (0..127)
  uint16_t lr = audio.getVUlevel();
  uint8_t l = (lr >> 8) & 0x7F;
  uint8_t r = (lr & 0xFF) & 0x7F;

  // map na počet rozsvícených segmentů 0..7
  uint8_t segL = constrain((int)l * VU_SEGMENTS / 128, 0, VU_SEGMENTS);
  uint8_t segR = constrain((int)r * VU_SEGMENTS / 128, 0, VU_SEGMENTS);

  if (segL != segL_last || segR != segR_last) {
    int rowH = (VU_H - VU_SEG_GAP) / 2;
    int baseY_L = VU_Y;
    int baseY_R = VU_Y + rowH + VU_SEG_GAP;

    drawVURow(segL, baseY_L, rowH);
    drawVURow(segR, baseY_R, rowH);

    segL_last = segL;
    segR_last = segR;
  }
}

// ============== Audio helpery ==============
void startPlaying() {
  gErrMsg = "";
  gMeta   = "";
  gStatus = ST_STOPPED;
  drawStatusLine();

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  if (audio.getVolume() > 21) audio.setVolume(12);

  waitingConnect = true;
  connStartMs    = millis();

  drawVUBackground();
  audio.connecttohost(stations[stIndex].url);
}

void stopPlaying() {
  audio.stopSong();
  waitingConnect = false;
  gStatus = ST_STOPPED;
  drawButtons();
  drawStatusLine();
}

void nextStation() {
  stopPlaying();
  stIndex = (stIndex + 1) % NUM_ST;
  drawStationName(stations[stIndex].name);
  startPlaying();
  drawButtons();
}

void prevStation() {
  stopPlaying();
  stIndex = (stIndex - 1 + NUM_ST) % NUM_ST;
  drawStationName(stations[stIndex].name);
  startPlaying();
  drawButtons();
}

void toggleBrightness() {
  backlightIdx = (backlightIdx + 1) % 5;
  analogWrite(TFT_BL, backlightSteps[backlightIdx]);
  drawBrightnessSun();
}

// všeobecné log/info hlášky – zde si vytáhneme bitrate/codec, pokud to knihovna pošle
void audio_info(const char* info) {
  String s = info ? String(info) : String("");
  if (s.length()) {
    String low = s; low.toLowerCase();

    // detekce chyb, jak už máš
    if (low.indexOf("error")>=0 || low.indexOf("fail")>=0 ||
        low.indexOf("lost")>=0  || low.indexOf("timeout")>=0 ||
        low.indexOf("invalid")>=0 || low.indexOf("not audio")>=0) {
      gErrMsg = s;
      gStatus = ST_ERROR;
      waitingConnect = false;
      drawStatusLine();
    }

    // hrubé parsování bitrate (knihovna často hlásí "bitrate: 128" apod.)
    int bi = low.indexOf("bitrate");
    if (bi >= 0) {
      // najdi čísla za "bitrate"
      int start = low.indexOf(':', bi);
      if (start < 0) start = low.indexOf(' ', bi);
      if (start >= 0) {
        start++;
        while (start < (int)low.length() && !isDigit(low[start])) start++;
        int end = start;
        while (end < (int)low.length() && isDigit(low[end])) end++;
        if (end > start) {
          gBitrate = low.substring(start, end) + " kbps";
          drawStreamInfoLine();
        }
      }
    }

    // parsování typu/kodeku: hledej klíčová slova
    if (low.indexOf("aac") >= 0)        { gCodec = "AAC";        drawStreamInfoLine(); }
    else if (low.indexOf("mp3") >= 0)   { gCodec = "MP3";        drawStreamInfoLine(); }
    else if (low.indexOf("opus") >= 0)  { gCodec = "OPUS";       drawStreamInfoLine(); }
    else if (low.indexOf("ogg") >= 0)   { gCodec = "OGG";        drawStreamInfoLine(); }
    else if (low.indexOf("mpeg") >= 0)  { gCodec = "MPEG";       drawStreamInfoLine(); }
    else if (low.indexOf("audio/") >= 0) {
      // obecný MIME typ "audio/xyz"
      int a = low.indexOf("audio/");
      int b = a >= 0 ? low.indexOf(' ', a) : -1;
      if (a >= 0) {
        String mime = (b > a) ? low.substring(a, b) : low.substring(a);
        mime.trim();
        mime.toUpperCase();
        gCodec = mime;
        drawStreamInfoLine();
      }
    }
  }
}
// přichází při změně titulu (Shoutcast/Icecast)
void audio_showstreamtitle(const char* title) {
  gSongTitle = title ? String(title) : "";
  drawSongTitleStatic();   // přepočítá šířku, vykreslí/rozjede marquee
}

void audio_id3data(const char*) {}
void audio_eof_speech(const char*) {}

// ============== Wi-Fi / displej init ==============
void StartWiFi() {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextFont(2);
  tft.drawString("Connecting to Wi-Fi...", TEXTPADDING, BRIGHT_Y + 6 + 2*18);

  WiFi.begin(SSID, PASSWORD);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    tft.drawString(".", 200, BRIGHT_Y + 6 + 2*18);
    if (millis() - t0 > 15000) {
      gStatus = ST_ERROR;
      gErrMsg = "Wi-Fi timeout";
      drawStatusLine();
      return;
    }
  }

  tft.fillRect(0, BRIGHT_Y + 6 + 2*18-2, TFT_RES_X, 40, COL_BG);
  tft.drawString(String("Connected: ") + SSID, TEXTPADDING, BRIGHT_Y + 6 + 2*18);
  tft.drawString(String("IP address: ") + WiFi.localIP().toString(), TEXTPADDING, BRIGHT_Y + 6 + 3*18);
}

void updateTitleMarquee() {
  if (titleTextW <= TITLE_AREA_W || gSongTitle.length() == 0) return;

  if (millis() - titleLastMs < TITLE_SPEED_MS) return;
  titleLastMs = millis();

  // vykreslovací oblast
  int areaX = TEXTPADDING;
  int areaY = TITLE_Y;
  int areaW = TITLE_AREA_W;
  int areaH = 16; // font 2 výška řádku

  // vyčisti oblast
  tft.fillRect(areaX, areaY-2, areaW, areaH+4, COL_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextFont(2);

  // posun textu
  titleScrollX -= 1; // krok v pixelech
  int totalCycle = titleTextW + TITLE_GAP_PX;
  if (-titleScrollX > totalCycle) titleScrollX = 0;

  // vykresli 2x za sebou (loop efekt)
  tft.setCursor(areaX + titleScrollX, areaY, 2);
  tft.print(gSongTitle);
  tft.setCursor(areaX + titleScrollX + totalCycle, areaY, 2);
  tft.print(gSongTitle);
}
void drawSongTitleStatic() {
  // vyčistit pás pro title
  tft.fillRect(0, TITLE_Y-2, TFT_RES_X, 18, COL_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextFont(2);

  String toDraw = gSongTitle;
  if (toDraw.length() == 0) toDraw = "-";   // placeholder když nepřišlo metadata

  titleTextW = tft.textWidth(toDraw, 2);
  if (titleTextW <= TITLE_AREA_W) {
    tft.drawString(toDraw, TEXTPADDING, TITLE_Y);
    titleScrollX = 0;
  } else {
    titleScrollX = 0;
    // první snímek posunu – aby bylo něco vidět hned
    tft.setCursor(TEXTPADDING + titleScrollX, TITLE_Y, 2);
    tft.print(toDraw);
  }
}

void drawStreamInfoLine() {
  // vyčistit pás pro info
  tft.fillRect(0, INFO_Y-2, TFT_RES_X, 18, COL_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextFont(2);

  String infoLine;
  if (gBitrate.length()) infoLine += gBitrate;              // např. "128 kbps"
  if (gCodec.length())   infoLine += (infoLine.length()? "  ·  " : "") + gCodec;

  if (infoLine.length() == 0) infoLine = "-";               // placeholder

  tft.drawString(infoLine, TEXTPADDING, INFO_Y);
}

// ============== setup / loop ==============
void setup() {
  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, backlightSteps[backlightIdx]);

  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!ts.begin(40)) {
    Serial.println("FT6236 init failed");
  }
  ts.setRotation(2);

  tft.init();
  tft.setRotation(0);
  tft.setSwapBytes(true);
  tft.setTextWrap(false, false);  // ať se dlouhé texty nezalamují do dalšího řádku

  drawHeader();
  drawBrightnessSun();
  drawStatusLine();
  StartWiFi();

  drawVolumeBar(12);
  audio.setVolume(12);
  drawStationName(stations[stIndex].name);
  drawSongTitleStatic();
  drawStreamInfoLine();
  drawButtons();
  drawBattery();

  drawVUBackground();
}

void loop() {
  updateVU();
  audio.loop();
  updateTitleMarquee();

  if (waitingConnect) {
    if (audio.isRunning()) {
      waitingConnect = false;
      gStatus = ST_PLAYING;
      drawButtons();
      drawStatusLine();
    } else if (millis() - connStartMs > CONNECT_TIMEOUT_MS) {
      waitingConnect = false;
      gStatus = ST_ERROR;
      gErrMsg = "Connect timeout";
      audio.stopSong();
      drawButtons();
      drawStatusLine();
    }
  } else {
    static uint32_t lastMs = 0;
    if (audio.isRunning()) {
      if (gStatus != ST_PLAYING) {
        gStatus = ST_PLAYING;
        drawButtons();
        drawStatusLine();
      }
      if (millis() - lastMs > 1000) {
        lastMs = millis();
        Serial.printf("Streaming %lu ms...\n", lastMs);
      }
    } else if (gStatus == ST_PLAYING) {
      gStatus = ST_STOPPED;
      drawButtons();
      drawStatusLine();
    }
  }

  if (ts.touched()) {
    TS_Point p = ts.getPoint();

    int gap = (TFT_RES_X - 3*BTN_W) / 4;
    int x1 = gap;
    int x2 = x1 + BTN_W + gap;
    int x3 = x2 + BTN_W + gap;

    if (hit(p.x, p.y, x2, BTN_Y, BTN_W, BTN_H)) {
      if (gStatus == ST_PLAYING || waitingConnect) stopPlaying();
      else                                         startPlaying();
      drawButtons();
      delay(160);
    }
    else if (hit(p.x, p.y, x3, BTN_Y, BTN_W, BTN_H)) {
      nextStation();
      delay(160);
    }
    else if (hit(p.x, p.y, x1, BTN_Y, BTN_W, BTN_H)) {
      prevStation();
      delay(160);
    } 
    else if (hit(p.x, p.y, PADDING, VOL_Y, (TFT_RES_X - 2*PADDING), VOL_H)) {
      int rel = p.x - PADDING;
      rel = constrain(rel, 0, (TFT_RES_X - 2*PADDING));
      uint8_t vol = map(rel, 0, (TFT_RES_X - 2*PADDING), 0, 21);
      audio.setVolume(vol);
      drawVolumeBar(vol);
      delay(100);
    }
    else if (hit(p.x, p.y, TFT_RES_X - 80, BRIGHT_Y, 80, BRIGHT_H)) {
      toggleBrightness();
      delay(120);
    }

    drawBattery();
  }
}
