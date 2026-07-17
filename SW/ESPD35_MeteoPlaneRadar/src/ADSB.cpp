// =============================================================================
//  ESPD35_MeteoPlaneRadar - ADS-B klient (stahovani dat o letadlech z adsb.fi).
//  Prevzato beze zmeny z petus/Meteo-PlaneRadar.
//  Zdroj: adsb.fi - zdarma, bez klice, jen pro osobni nekomercni pouziti.
// =============================================================================
#include "ADSB.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>

static const float KM_PER_NM = 1.852f;

static Aircraft s_list[ADSB_MAX];
static int s_count = 0;
static void (*s_poll)() = nullptr;

void ADSB_SetPollFn(void (*fn)()) { s_poll = fn; }
int  ADSB_Count() { return s_count; }
const Aircraft* ADSB_List() { return s_list; }

// Cte cislo i kdyz je v JSON jako retezec.
static bool readFloat(JsonObjectConst o, const char* key, float* out) {
  JsonVariantConst v = o[key];
  if (v.is<float>() || v.is<double>() || v.is<int>()) { *out = v.as<float>(); return true; }
  if (v.is<const char*>()) {
    const char* s = v.as<const char*>();
    if (s && *s) { *out = (float)atof(s); return true; }
  }
  return false;
}

static void copyCallsign(Aircraft* a, JsonObjectConst plane) {
  const char* flight = plane["flight"] | "";
  const char* hex = plane["hex"] | "";
  const char* src = (flight[0] != '\0') ? flight : hex;
  while (*src == ' ') src++;   // preskoc uvodni mezery
  int i = 0;
  while (src[i] && i < (int)sizeof(a->callsign) - 1) { a->callsign[i] = src[i]; i++; }
  while (i > 0 && a->callsign[i-1] == ' ') i--;   // orizni koncove
  a->callsign[i] = '\0';
}

bool ADSB_Fetch(double lat, double lon, float radiusKm) {
  if (WiFi.status() != WL_CONNECTED) { s_count = 0; return false; }

  float distNm = radiusKm / KM_PER_NM;
  char url[128];
  snprintf(url, sizeof(url), "%s%.5f/lon/%.5f/dist/%.1f",
           ADSB_API_BASE, lat, lon, distNm);

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(client, url)) { s_count = 0; return false; }
  http.useHTTP10(true);   // pro streamovany parser
  int code = http.GET();
  if (code != HTTP_CODE_OK) { http.end(); s_count = 0; return false; }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err) { s_count = 0; return false; }

  JsonArrayConst ac = doc["ac"].as<JsonArrayConst>();
  int n = 0;
  for (JsonObjectConst plane : ac) {
    float plat, plon;
    if (!readFloat(plane, "lat", &plat) || !readFloat(plane, "lon", &plon)) continue;
    if (n >= ADSB_MAX) break;
    s_list[n].lat = plat;
    s_list[n].lon = plon;
    // Smer letu - poznamename, jestli vubec existuje.
    float tr = 0;
    if (readFloat(plane, "track", &tr) || readFloat(plane, "true_heading", &tr)) {
      s_list[n].track = tr;
      s_list[n].hasTrack = true;
    } else {
      s_list[n].track = 0;
      s_list[n].hasTrack = false;
    }
    // Vyska (baro), rychlost, stoupani.
    JsonVariantConst ab = plane["alt_baro"];
    s_list[n].onGround = ab.is<const char*>() && strcmp(ab.as<const char*>(), "ground") == 0;
    float f = 0;
    s_list[n].altFt = (!s_list[n].onGround && readFloat(plane, "alt_baro", &f)) ? f : 0;
    s_list[n].gsKt = readFloat(plane, "gs", &f) ? f : 0;
    s_list[n].baroRate = readFloat(plane, "baro_rate", &f) ? f : 0;
    // Typ letadla (ruzne klice dle zdroje).
    const char* ty = plane["t"] | (plane["type"] | "");
    strncpy(s_list[n].type, ty, sizeof(s_list[n].type) - 1);
    s_list[n].type[sizeof(s_list[n].type) - 1] = '\0';
    // ICAO hex - stabilni identifikator (nemeni se mezi stazenimi).
    const char* hx = plane["hex"] | "";
    strncpy(s_list[n].icao, hx, sizeof(s_list[n].icao) - 1);
    s_list[n].icao[sizeof(s_list[n].icao) - 1] = '\0';
    copyCallsign(&s_list[n], plane);
    n++;
  }
  s_count = n;
  Serial.printf("Letadla: %d\n", n);
  return true;
}
