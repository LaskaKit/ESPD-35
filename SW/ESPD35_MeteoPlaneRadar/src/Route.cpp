// =============================================================================
//  MeteoPlaneRadar - flight route + airframe lookup (adsbdb.com).
//  See Route.h for the reasoning.
//
// =============================================================================
#include "Route.h"
#include "Config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <string.h>

// --- Text sanitising --------------------------------------------------------
// adsbdb returns proper Unicode city names - "Izmir" really comes back as
// "\u0130zmir" with a Turkish dotted capital I, which is two bytes in UTF-8.
// The built-in GFX font is 7-bit, so those bytes came out as two random glyphs
// and the display showed something like "-?zmir". Everything is therefore
// folded down to ASCII: accents are dropped, the letter underneath is kept.
static const char LAT1_MAP[65]  = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";   // U+00C0..U+00FF
static const char LATA_MAP[129] = "AaAaAaCcCcCcCcDdDdEeEeEeEeEeGgGgGgGgHhHhIiIiIiIiIiIiJjKkkLlLlLlLlLlNnNnNnnNnOoOoOoOoRrRrRrSsSsSsSsTtTtTtUuUuUuUuUuUuWwYyYZzZzZzs";   // U+0100..U+017F

static void toAscii(char* dst, size_t cap, const char* src) {
  size_t o = 0;
  for (const unsigned char* p = (const unsigned char*)src; *p && o + 1 < cap; ) {
    unsigned char c = *p;
    uint32_t cp;
    if (c < 0x80)             { cp = c;                                p += 1; }
    else if ((c & 0xE0) == 0xC0 && p[1]) { cp = ((c & 0x1F) << 6) | (p[1] & 0x3F);  p += 2; }
    else if ((c & 0xF0) == 0xE0 && p[1] && p[2]) {
      cp = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);  p += 3;
    } else { p += 1; continue; }            // broken byte - skip it

    char out;
    if (cp < 0x80)                       out = (char)cp;
    else if (cp >= 0xC0 && cp <= 0xFF)   out = LAT1_MAP[cp - 0xC0];
    else if (cp >= 0x100 && cp <= 0x17F) out = LATA_MAP[cp - 0x100];
    else                                 continue;   // not worth a guess
    if (out < 0x20) continue;
    dst[o++] = out;
  }
  dst[o] = '\0';
}

// --- Cache ------------------------------------------------------------------
// Keyed on the callsign, or on the hex when there is no callsign. Round-robin
// replacement: with this few entries anything cleverer costs more than it saves.
struct Entry {
  char       key[12] = "";
  RouteState state    = ROUTE_IDLE;
  RouteInfo  info;
};
static Entry s_cache[ROUTE_CACHE_N];
static int   s_next = 0;

static char s_wantKey[12]  = "";   // what the UI is showing right now
static char s_wantCall[12] = "";
static char s_wantHex[10]  = "";
static bool s_pending = false;

static Entry* find(const char* key) {
  if (!key || !*key) return nullptr;
  for (int i = 0; i < ROUTE_CACHE_N; i++)
    if (strncmp(s_cache[i].key, key, sizeof(s_cache[i].key)) == 0) return &s_cache[i];
  return nullptr;
}

static Entry* insert(const char* key) {
  Entry* e = &s_cache[s_next];
  s_next = (s_next + 1) % ROUTE_CACHE_N;
  memset(e, 0, sizeof(*e));
  strncpy(e->key, key, sizeof(e->key) - 1);
  e->state = ROUTE_WAIT;
  return e;
}

// Trim trailing blanks - adsb.fi pads callsigns to eight characters.
static void trim(char* s) {
  int n = strlen(s);
  while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t')) s[--n] = '\0';
}

void Route_Select(const char* callsign, const char* hex) {
  char call[12] = "";
  if (callsign) strncpy(call, callsign, sizeof(call) - 1);
  trim(call);

  char key[12] = "";
  strncpy(key, call[0] ? call : (hex ? hex : ""), sizeof(key) - 1);
  if (!key[0]) { Route_Clear(); return; }
  if (strcmp(key, s_wantKey) == 0) return;      // already showing this one

  strncpy(s_wantKey,  key,  sizeof(s_wantKey) - 1);
  strncpy(s_wantCall, call, sizeof(s_wantCall) - 1);
  s_wantHex[0] = '\0';
  if (hex) strncpy(s_wantHex, hex, sizeof(s_wantHex) - 1);

  if (find(key)) { s_pending = false; return; }  // answered before
  insert(key);
  s_pending = true;
}

void Route_Clear() {
  s_wantKey[0] = s_wantCall[0] = s_wantHex[0] = '\0';
  s_pending = false;
}

RouteState Route_GetState() {
  Entry* e = find(s_wantKey);
  return e ? e->state : ROUTE_IDLE;
}

const RouteInfo* Route_Get() {
  Entry* e = find(s_wantKey);
  return (e && e->state == ROUTE_OK) ? &e->info : nullptr;
}

// --- Fetching ---------------------------------------------------------------
static void copyField(char* dst, size_t cap, JsonVariant primary, JsonVariant fallback) {
  const char* v = primary.is<const char*>() ? primary.as<const char*>() : nullptr;
  if (!v || !*v) v = fallback.is<const char*>() ? fallback.as<const char*>() : nullptr;
  if (!v) return;
  toAscii(dst, cap, v);      // the font cannot draw anything but ASCII
}

// One GET with a filter. Returns true when the document was parsed.
static bool getJson(const char* url, JsonDocument& filter, JsonDocument& doc) {
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(6000);
  http.setTimeout(8000);
  http.setReuse(false);
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  if (code != HTTP_CODE_OK) { http.end(); return false; }   // 404 = not on file
  DeserializationError err = deserializeJson(doc, http.getStream(),
                                             DeserializationOption::Filter(filter));
  http.end();
  return !err;
}

void Route_Tick() {
  if (!s_pending) return;
  if (WiFi.status() != WL_CONNECTED) return;

  // Same rule as everywhere else: no TLS handshake without room for it.
  size_t freeInt = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (freeInt < NET_MIN_HEAP) return;    // try again on the next pass

  s_pending = false;
  Entry* e = find(s_wantKey);
  if (!e) return;

  bool any = false;
  char url[128];

  // 1) The route, by callsign. Municipality names ("London") read better than
  //    airport codes - though they are NOT always ASCII, hence toAscii().
  if (s_wantCall[0]) {
    JsonDocument filter;
    filter["response"]["flightroute"]["origin"]["municipality"] = true;
    filter["response"]["flightroute"]["origin"]["iata_code"]    = true;
    filter["response"]["flightroute"]["destination"]["municipality"] = true;
    filter["response"]["flightroute"]["destination"]["iata_code"]    = true;
    JsonDocument doc;
    snprintf(url, sizeof(url), "%s/v0/callsign/%s", ROUTE_API_BASE, s_wantCall);
    if (getJson(url, filter, doc)) {
      JsonVariant fr = doc["response"]["flightroute"];
      if (!fr.isNull()) {
        copyField(e->info.from, sizeof(e->info.from),
                  fr["origin"]["municipality"], fr["origin"]["iata_code"]);
        copyField(e->info.to, sizeof(e->info.to),
                  fr["destination"]["municipality"], fr["destination"]["iata_code"]);
        if (e->info.from[0] || e->info.to[0]) any = true;
      }
    }
  }

  // 2) The airframe, by ICAO hex - registration and type, which adsb.fi only
  //    reports sometimes.
  if (s_wantHex[0] && s_wantHex[0] != '~') {
    JsonDocument filter;
    filter["response"]["aircraft"]["registration"] = true;
    // icao_type is the short code ("A332"), type the wordy version
    // ("A330 202"). The panel has room for the code, not for the sentence.
    filter["response"]["aircraft"]["icao_type"]    = true;
    JsonDocument doc;
    snprintf(url, sizeof(url), "%s/v0/aircraft/%s", ROUTE_API_BASE, s_wantHex);
    if (getJson(url, filter, doc)) {
      JsonVariant ac = doc["response"]["aircraft"];
      if (!ac.isNull()) {
        copyField(e->info.reg,  sizeof(e->info.reg),  ac["registration"], JsonVariant());
        copyField(e->info.type, sizeof(e->info.type), ac["icao_type"],    JsonVariant());
        if (e->info.reg[0] || e->info.type[0]) any = true;
      }
    }
  }

  e->state = any ? ROUTE_OK : ROUTE_NONE;
  Serial.printf("TRASA %s: %s\n", e->key,
                any ? "nalezeno" : "neni v databazi");
}
