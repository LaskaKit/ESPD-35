// =============================================================================
//  ESPD35_MeteoPlaneRadar - meteoradar CHMU: stahovani do PSRAM (1 snimek + animace).
// =============================================================================
#include "CHMU.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "esp_heap_caps.h"
#include "Net.h"      // sdilene stahovani, strazce pameti, keep-alive relace
#include "Status.h"   // jednoradkove hlaseni pro stavovou stranku

static const char* NAME_PREFIX = "pacz2gmaps3.z_max3d.";

static void (*s_poll)() = nullptr;
static void poll() { if (s_poll) s_poll(); }
void CHMU_SetPollFn(void (*fn)()) { s_poll = fn; }

// -----------------------------------------------------------------------------
//  Spolecne pomucky
// -----------------------------------------------------------------------------
static String extractTimestamp(const String& name) {
  int start = name.indexOf(NAME_PREFIX);
  if (start < 0) return "";
  int ds = start + strlen(NAME_PREFIX);
  if ((int)name.length() < ds + 13) return "";
  if (name[ds + 8] != '.') return "";
  String date = name.substring(ds, ds + 8);
  String hhmm = name.substring(ds + 9, ds + 13);
  for (unsigned i = 0; i < date.length(); i++) if (!isDigit(date[i])) return "";
  for (unsigned i = 0; i < hhmm.length(); i++) if (!isDigit(hhmm[i])) return "";
  return date + hhmm;   // YYYYMMDDHHMM
}

static String timeTextFromName(const String& name) {
  String ts = extractTimestamp(name);
  if (ts.length() < 12) return "";
  int hh = ts.substring(8, 10).toInt();
  int mm = ts.substring(10, 12).toInt();
  time_t now = time(nullptr);
  struct tm lt; localtime_r(&now, &lt);
  struct tm gt; gmtime_r(&now, &gt);
  int off = lt.tm_hour - gt.tm_hour;
  if (off < -12) off += 24; if (off > 12) off -= 24;
  hh = (hh + off + 24) % 24;
  char out[6];
  snprintf(out, sizeof(out), "%02d:%02d", hh, mm);
  return String(out);
}

// Stahne dany PNG do zadaneho bufferu. Vraci true a naplni *outSize.
//
// Od 0.4.0 pres Net_GetBinary, takze plati totez, co pro ostatni stahovani:
// kontrola volne interni pameti pred TLS handshakem, hlavicka Date pro hodiny
// a hlavne - NEUPLNY prenos je CHYBA. Driv se vracelo cokoli nad 100 bajtu,
// takze uriznuty PNG propadl do dekoderu a projevil se jako pruh smeti pres
// snimek. Uvnitr relace (viz CHMU_FetchAnim) navic vsechny snimky sdileji
// jedno spojeni misto sedmi handshaku za sebou.
static bool downloadNameTo(const String& name, uint8_t* buf, size_t cap, size_t* outSize) {
  *outSize = 0;
  if (!buf) return false;
  String url = String(CHMU_INDEX_URL) + name;
  return Net_GetBinary(url.c_str(), buf, cap, outSize, "CHMU");
}

// -----------------------------------------------------------------------------
//  Jeden (nejnovejsi) snimek - puvodni API
// -----------------------------------------------------------------------------
static String   s_lastName;
static bool     s_hasSnapshot = false;
static uint8_t* s_pngBuf = nullptr;
static size_t   s_pngSize = 0;

bool     CHMU_HasSnapshot() { return s_hasSnapshot; }
uint8_t* CHMU_Data() { return s_pngBuf; }
size_t   CHMU_DataSize() { return s_pngSize; }

// Vysledek scanChunkLatest. Ve statikach, aby sel scanner predat spolecnemu
// stahovaci indexu nize jako obycejny ukazatel na funkci.
static String s_newestTs;
static String s_latestName;

static void scanChunkLatest(const String& text) {
  int pos = 0;
  while (true) {
    int idx = text.indexOf(NAME_PREFIX, pos); if (idx < 0) break;
    int end = text.indexOf(".png", idx); if (end < 0) break;
    String name = text.substring(idx, end + 4);
    String ts = extractTimestamp(name);
    if (ts.length() && ts > s_newestTs) { s_newestTs = ts; s_latestName = name; }
    pos = end + 4;
  }
}

// -----------------------------------------------------------------------------
//  Stazeni a proskenovani indexu CHMU
//
//  Vypis adresare je dlouhy, takze se nikdy nedrzi cely v pameti: cte se po
//  kouscich do klouzaveho okna a scanner se vola prubezne. Okno se orezava na
//  250 znaku, coz je vic nez nejdelsi nazev souboru - jmeno rozdelene mezi dve
//  cteni se tim padem najde.
//
//  Stejnou praci potrebuji obe cesty (jeden snimek i animace), lisi se jen
//  scannerem - proto jeden spolecny kus a ukazatel na funkci.
// -----------------------------------------------------------------------------
static bool fetchIndex(void (*scan)(const String&)) {
  if (WiFi.status() != WL_CONNECTED) { Status_Set(ST_RADAR, "bez WiFi"); return false; }
  if (!Net_HeapOk("CHMU")) { Status_Set(ST_RADAR, "malo pameti"); return false; }

  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(15000);
  if (!http.begin(client, CHMU_INDEX_URL)) { Status_Set(ST_RADAR, "begin() selhalo"); return false; }
  http.collectHeaders(NET_DATE_HEADER, 1);

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    Serial.printf("CHMU: index HTTP %d\n", code);
    Status_Set(ST_RADAR, "index HTTP %d", code);
    return false;
  }
  Net_NoteDate(http);   // hlavicka Date je zaloha hodin (viz Clock.h)

  WiFiClient* stream = http.getStreamPtr();
  if (!stream) { http.end(); return false; }

  String window;
  uint8_t buf[512];
  unsigned long last = millis();
  while (http.connected()) {
    poll();
    size_t avail = stream->available();
    if (avail) {
      int n = stream->readBytes(buf, avail < sizeof(buf) ? avail : sizeof(buf));
      if (n <= 0) break;
      window.concat((const char*)buf, n);
      scan(window);
      if (window.length() > 400) window = window.substring(window.length() - 250);
      last = millis();
    } else {
      if (millis() - last > 10000) break;
      delay(1);
    }
  }
  scan(window);
  http.end();
  return true;
}

bool CHMU_FetchLatest() {
  s_newestTs = "";
  s_latestName = "";
  if (!fetchIndex(scanChunkLatest)) return s_hasSnapshot;
  if (s_latestName.isEmpty()) {
    Status_Set(ST_RADAR, "index bez snimku");
    return s_hasSnapshot;
  }
  if (s_latestName == s_lastName && s_hasSnapshot) return true;

  if (!s_pngBuf) {
    s_pngBuf = (uint8_t*)heap_caps_malloc(CHMU_MAX_PNG, MALLOC_CAP_SPIRAM);
    if (!s_pngBuf) s_pngBuf = (uint8_t*)malloc(CHMU_MAX_PNG);
  }
  if (!s_pngBuf) { Status_Set(ST_RADAR, "nedostatek pameti"); return s_hasSnapshot; }

  if (downloadNameTo(s_latestName, s_pngBuf, CHMU_MAX_PNG, &s_pngSize)) {
    s_lastName = s_latestName;
    s_hasSnapshot = true;
    Status_Set(ST_RADAR, "OK, 1 snimek");
    return true;
  }
  Status_Set(ST_RADAR, "snimek se nestahl");
  return s_hasSnapshot;
}

String CHMU_SnapshotTimeText() { return timeTextFromName(s_lastName); }

// -----------------------------------------------------------------------------
//  Animace - nejnovejsich N ramcu
// -----------------------------------------------------------------------------
static uint8_t* s_animBuf[CHMU_ANIM_MAX] = {0};
static size_t   s_animSize[CHMU_ANIM_MAX] = {0};
static String   s_animName[CHMU_ANIM_MAX];
static int      s_animCount = 0;

int      CHMU_AnimCount() { return s_animCount; }
uint8_t* CHMU_AnimData(int i) { return (i >= 0 && i < s_animCount) ? s_animBuf[i] : nullptr; }
size_t   CHMU_AnimSize(int i) { return (i >= 0 && i < s_animCount) ? s_animSize[i] : 0; }
String   CHMU_AnimTimeText(int i) { return (i >= 0 && i < s_animCount) ? timeTextFromName(s_animName[i]) : String(""); }

// Bezici "top-N" nejnovejsich nazvu (vzestupne dle casu).
static String s_topName[CHMU_ANIM_MAX];
static String s_topTs[CHMU_ANIM_MAX];
static int    s_topCount = 0;

static void topInsert(const String& name, const String& ts) {
  for (int i = 0; i < s_topCount; i++) if (s_topTs[i] == ts) return;   // duplicita
  if (s_topCount < CHMU_ANIM_MAX) {
    int p = s_topCount;
    while (p > 0 && s_topTs[p - 1] > ts) { s_topTs[p] = s_topTs[p - 1]; s_topName[p] = s_topName[p - 1]; p--; }
    s_topTs[p] = ts; s_topName[p] = name; s_topCount++;
  } else if (ts > s_topTs[0]) {   // nahradime nejstarsi
    int p = 0;
    while (p < CHMU_ANIM_MAX - 1 && s_topTs[p + 1] < ts) { s_topTs[p] = s_topTs[p + 1]; s_topName[p] = s_topName[p + 1]; p++; }
    s_topTs[p] = ts; s_topName[p] = name;
  }
}

static void scanChunkTop(const String& text) {
  int pos = 0;
  while (true) {
    int idx = text.indexOf(NAME_PREFIX, pos); if (idx < 0) break;
    int end = text.indexOf(".png", idx); if (end < 0) break;
    String name = text.substring(idx, end + 4);
    String ts = extractTimestamp(name);
    if (ts.length()) topInsert(name, ts);
    pos = end + 4;
  }
}

static bool ensureAnimBuffer(int i) {
  if (s_animBuf[i]) return true;
  s_animBuf[i] = (uint8_t*)heap_caps_malloc(CHMU_MAX_PNG, MALLOC_CAP_SPIRAM);
  if (!s_animBuf[i]) s_animBuf[i] = (uint8_t*)malloc(CHMU_MAX_PNG);
  return s_animBuf[i] != nullptr;
}

int CHMU_FetchAnim(int wantN) {
  if (WiFi.status() != WL_CONNECTED) { Status_Set(ST_RADAR, "bez WiFi"); return s_animCount; }
  if (wantN > CHMU_ANIM_MAX) wantN = CHMU_ANIM_MAX;
  if (wantN < 1) wantN = 1;

  // 1) projdi index a najdi N nejnovejsich nazvu
  s_topCount = 0;
  if (!fetchIndex(scanChunkTop)) return s_animCount;
  if (s_topCount == 0) { Status_Set(ST_RADAR, "index bez snimku"); return s_animCount; }

  // 2) stahni N nejnovejsich (top pole je vzestupne, bereme konec)
  //
  // Vsechny snimky v JEDNE relaci: bez ni to je sedm TLS handshaku za sebou
  // (index + sest PNG) a kazdy z nich chce ~45 kB interni RAM. Az na pozadi
  // pobezi web server (0.5.0), byla by to presne ta spicka, ktera se nevejde.
  Net_SessionBegin();
  int n = s_topCount < wantN ? s_topCount : wantN;
  int startIdx = s_topCount - n;
  int got = 0;
  for (int i = 0; i < n; i++) {
    if (!ensureAnimBuffer(i)) break;
    size_t sz = 0;
    if (downloadNameTo(s_topName[startIdx + i], s_animBuf[i], CHMU_MAX_PNG, &sz)) {
      s_animSize[i] = sz;
      s_animName[i] = s_topName[startIdx + i];
      got++;
    } else break;
  }
  Net_SessionEnd();

  // Castecne stazena animace se NEZAHAZUJE - ctyri snimky jsou porad lepsi nez
  // zadny. s_animCount se ale prepise az tady, takze se nikdy nezobrazi ramec
  // s daty z predchoziho cyklu.
  s_animCount = got;
  Serial.printf("Meteoradar: %d ramcu\n", got);
  if (got == 0)          Status_Set(ST_RADAR, "zadny snimek se nestahl");
  else if (got < n)      Status_Set(ST_RADAR, "OK, %d z %d ramcu", got, n);
  else                   Status_Set(ST_RADAR, "OK, %d ramcu", got);
  return got;
}
