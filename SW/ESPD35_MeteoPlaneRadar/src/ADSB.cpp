// =============================================================================
//  ESPD35_MeteoPlaneRadar - ADS-B klient (stahovani dat o letadlech z adsb.fi).
//  Robustni stahovani: cele telo se nacte do PSRAM bufferu a parsuje se az
//  kompletni (kontrola utnuti proti Content-Length + jeden retry), s filtrem
//  ArduinoJson v7. Pri chybe zustava posledni dobry snimek (s_list/s_count se
//  neprepise), takze uriznuty JSON uz radar nevymaze.
//  Zdroj: adsb.fi - zdarma, bez klice, jen pro osobni nekomercni pouziti.
// =============================================================================
#include "ADSB.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>
#include <string.h>
#include "esp_heap_caps.h"   // PSRAM body buffer

static const float KM_PER_NM = 1.852f;

static Aircraft s_list[ADSB_MAX];   // posledni DOBRY snimek na obrazovce
static int s_count = 0;
// Docasny buffer, do ktereho parsujeme. Viditelny seznam (s_list/s_count) se
// prepise az po plnem a spravnem parsu - tak uriznuty JSON nikdy nevymaze radar.
static Aircraft s_tmp[ADSB_MAX];
// Ctverec vzdalenosti kazdeho letadla v s_tmp od stredu (km^2). Slouzi k tomu,
// aby se pri prekroceni ADSB_MAX drzela NEJBLIZSI letadla, ne prvnich sto
// v poradi, ve kterem je posle server (viz smycku parsovani nize).
static float s_tmpD2[ADSB_MAX];
static void (*s_poll)() = nullptr;

void ADSB_SetPollFn(void (*fn)()) { s_poll = fn; }
int  ADSB_Count() { return s_count; }
const Aircraft* ADSB_List() { return s_list; }

int ADSB_FindByIcao(const char* icao) {
  if (!icao || !icao[0]) return -1;
  for (int i = 0; i < s_count; i++) {
    if (strcmp(s_list[i].icao, icao) == 0) return i;
  }
  return -1;   // uz neni v datech
}

static void poll() { if (s_poll) s_poll(); }

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

// -----------------------------------------------------------------------------
//  Buffer tela (PSRAM). Recykluje se mezi stazenimi, aby se interni heap
//  netristil velkym String pri kazdem pollu. Cele telo se nacte SEM pred
//  parsovanim, takze parser vzdy vidi kompletni dokument (puvodni
//  "IncompleteInput" vznikal parsovanim primo z TLS streamu, ktery skoncil
//  uprostred objektu).
// -----------------------------------------------------------------------------
static char*  s_body    = nullptr;
static size_t s_bodyCap = 0;
static const size_t ADSB_MAX_BODY = 1024 * 1024;   // 1 MB tvrdy strop

static bool bodyReserve(size_t need) {
  if (need <= s_bodyCap) return true;
  size_t cap = need + 2048;
  char* nb = (char*)heap_caps_realloc(s_body, cap, MALLOC_CAP_SPIRAM);
  if (!nb) nb = (char*)heap_caps_realloc(s_body, cap, MALLOC_CAP_DEFAULT);
  if (!nb) return false;
  s_body = nb; s_bodyCap = cap;
  return true;
}

// Nacte cele telo HTTP do s_body. Vraci pocet bajtu (>= 0), nebo -1 pri tvrde
// chybe (alokace / neni stream). *complete = false, kdyz server deklaroval
// Content-Length, ktery jsme nedostali cely (uriznute stahovani).
static long readBody(HTTPClient& http, bool* complete) {
  *complete = true;
  int declared = http.getSize();              // -1 kdyz neznamy / chunked
  if (declared > (int)ADSB_MAX_BODY) return -1;
  WiFiClient* stream = http.getStreamPtr();
  if (!stream) return -1;

  size_t want = (declared > 0) ? (size_t)declared : 8192;
  if (!bodyReserve(want + 1)) return -1;

  size_t total = 0;
  unsigned long last = millis();
  while (http.connected() && (declared < 0 || total < (size_t)declared)) {
    poll();                                    // yield + nakrmi watchdog
    size_t avail = stream->available();
    if (avail) {
      if (total + avail + 1 > s_bodyCap) {
        if (total + avail + 1 > ADSB_MAX_BODY) { *complete = false; break; }
        if (!bodyReserve(total + avail + 1)) return -1;
      }
      int r = stream->readBytes(s_body + total, avail);
      if (r <= 0) break;
      total += r;
      last = millis();
    } else {
      if (millis() - last > 8000) break;       // zaseknuto uprostred prenosu
      delay(2);
    }
  }
  if (s_body) s_body[total] = '\0';
  if (declared > 0 && total < (size_t)declared) *complete = false;
  return (long)total;
}

// Filtr: parsuji se jen klice, ktere pouzivame, takze JsonDocument zustava maly
// bez ohledu na to, kolik adsb.fi posle. alt_baro MUSI zustat - nese literal
// "ground" pro detekci letadla na zemi.
static void buildFilter(JsonDocument& filter) {
  JsonObject o = filter["ac"].add<JsonObject>();   // ArduinoJson 7 idiom
  o["hex"]          = true;
  o["flight"]       = true;
  o["lat"]          = true;
  o["lon"]          = true;
  o["track"]        = true;
  o["true_heading"] = true;
  o["alt_baro"]     = true;
  o["gs"]           = true;
  o["baro_rate"]    = true;
  o["t"]            = true;
  o["type"]         = true;
}

bool ADSB_Fetch(double lat, double lon, float radiusKm) {
  // Bez pripojeni -> nech, co je na obrazovce (radar NEVYMAZAT).
  if (WiFi.status() != WL_CONNECTED) { Serial.println("ADSB: no WiFi"); return false; }

  float distNm = radiusKm / KM_PER_NM;
  char url[128];
  snprintf(url, sizeof(url), "%s%.5f/lon/%.5f/dist/%.1f",
           ADSB_API_BASE, lat, lon, distNm);
  Serial.printf("ADSB: %s\n", url);

  // Az dva pokusy. Jeden prechodny zadrhel (spadle TLS, uriznute telo) tak
  // nepreskoci cely cyklus; po dvou chybach se propadne a zustane posledni snimek.
  const int MAX_ATTEMPTS = 2;
  for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
    poll();
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setConnectTimeout(8000);   // ms - TCP + TLS handshake
    http.setTimeout(12000);         // ms - cteni
    http.setReuse(false);
    if (!http.begin(client, url)) {
      Serial.println("ADSB: begin failed");
      if (attempt < MAX_ATTEMPTS) { delay(200); continue; }
      return false;
    }
    // Slusne se predstavit - bezplatne API adsb.fi o to zada.
    http.addHeader("User-Agent", "ESPD35_MeteoPlaneRadar/1.0 (+https://chiptron.cz)");
    http.addHeader("Accept", "application/json");

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
      Serial.printf("ADSB: HTTP %d (pokus %d)\n", code, attempt);
      http.end();
      if (attempt < MAX_ATTEMPTS) { delay(200); continue; }
      return false;   // nech posledni dobra data
    }

    // Nacti CELE telo (do PSRAM), pak teprve parsuj - zadny parse ze streamu.
    bool complete = true;
    long len = readBody(http, &complete);
    http.end();

    if (len < 0) {
      Serial.println("ADSB: cteni tela selhalo");
      if (attempt < MAX_ATTEMPTS) { delay(200); continue; }
      return false;
    }
    if (!complete) {
      Serial.printf("ADSB: uriznute telo (pokus %d)\n", attempt);
      if (attempt < MAX_ATTEMPTS) { delay(200); continue; }
      return false;
    }
    if (len < 8) {
      Serial.printf("ADSB: kratke telo %ld (pokus %d)\n", len, attempt);
      if (attempt < MAX_ATTEMPTS) { delay(200); continue; }
      return false;
    }

    // Filtrovany parse kompletniho bufferu.
    JsonDocument filter;
    buildFilter(filter);
    JsonDocument doc;
    DeserializationError err =
        deserializeJson(doc, s_body, (size_t)len, DeserializationOption::Filter(filter));
    if (err) {
      Serial.printf("ADSB: JSON %s (pokus %d) - drzim poslednich %d\n",
                    err.c_str(), attempt, s_count);
      if (attempt < MAX_ATTEMPTS) { delay(200); continue; }
      return false;
    }

    if (!doc["ac"].is<JsonArray>()) {
      Serial.println("ADSB: chybi pole 'ac' - drzim posledni data");
      return false;   // validni JSON, ale spatny tvar; retry by nepomohl
    }

    // Parsuj do DOCASNEHO seznamu; do ziveho commitni az pri uspechu.
    //
    // Pri prekroceni ADSB_MAX se NEUTINA slepe. Puvodne se pri naplneni pole
    // proste skoncilo (break), takze o tom, ktera letadla uvidite, rozhodovalo
    // poradi v odpovedi serveru - u velkeho rozsahu nad hustym provozem tak
    // mohlo vypadnout letadlo primo nad vami. Nove se drzi NEJBLIZSICH
    // ADSB_MAX letadel: po naplneni pole se nove letadlo porovna s dosud
    // nejvzdalenejsim a pripadne ho nahradi.
    const float latr0 = (float)lat * 0.0174532925f;
    const float cosLat0 = cosf(latr0);

    JsonArrayConst ac = doc["ac"].as<JsonArrayConst>();
    int   n = 0;
    int   worstIdx = -1;      // slot s nejvetsi vzdalenosti (jen kdyz je plno)
    float worstD2  = -1.0f;
    int   dropped  = 0;       // kolik letadel se nevešlo (jen pro vypis)

    for (JsonObjectConst plane : ac) {
      float plat, plon;
      if (!readFloat(plane, "lat", &plat) || !readFloat(plane, "lon", &plon)) continue;
      // Ochrana proti nesmyslnym souradnicim (NaN projde i porovnanim, proto
      // se testuje pres !(a && b), ne pres opacnou podminku).
      if (!(plat >= -90.0f && plat <= 90.0f && plon >= -180.0f && plon <= 180.0f)) continue;
      if (plat == 0.0f && plon == 0.0f) continue;

      // Letadla NA ZEMI zahazujeme uz tady, aby nikdy nezabrala misto stroji
      // ve vzduchu. U letiste by jinak dokazala zaplnit cely ADSB_MAX.
      JsonVariantConst ab = plane["alt_baro"];
      if (ab.is<const char*>() && strcmp(ab.as<const char*>(), "ground") == 0) continue;

      // Vzdalenost od stredu (km^2, plocha azimutalni aproximace - staci nam
      // na porovnavani, nemusi byt presna).
      float dx = (plon - (float)lon) * 111.0f * cosLat0;
      float dy = (plat - (float)lat) * 111.0f;
      float d2 = dx * dx + dy * dy;

      int slot;
      if (n < ADSB_MAX) {
        slot = n++;
      } else {
        if (worstIdx < 0) {   // dopocitat nejvzdalenejsi az kdyz je poprve plno
          worstD2 = -1.0f;
          for (int i = 0; i < ADSB_MAX; i++)
            if (s_tmpD2[i] > worstD2) { worstD2 = s_tmpD2[i]; worstIdx = i; }
        }
        dropped++;
        if (d2 >= worstD2) continue;   // dal nez nejvzdalenejsi -> nezajima nas
        slot = worstIdx;
        worstIdx = -1;                 // po nahrazeni se musi najit znovu
      }

      s_tmpD2[slot] = d2;
      s_tmp[slot].lat = plat;
      s_tmp[slot].lon = plon;
      s_tmp[slot].onGround = false;
      // Smer letu - poznamename, jestli vubec existuje.
      float tr = 0;
      if (readFloat(plane, "track", &tr) || readFloat(plane, "true_heading", &tr)) {
        s_tmp[slot].track = tr;
        s_tmp[slot].hasTrack = true;
      } else {
        s_tmp[slot].track = 0;
        s_tmp[slot].hasTrack = false;
      }
      // Vyska (baro), rychlost, stoupani.
      float f = 0;
      s_tmp[slot].altFt    = readFloat(plane, "alt_baro", &f) ? f : 0;
      s_tmp[slot].gsKt     = readFloat(plane, "gs", &f) ? f : 0;
      s_tmp[slot].baroRate = readFloat(plane, "baro_rate", &f) ? f : 0;
      // Typ letadla (ruzne klice dle zdroje).
      const char* ty = plane["t"] | (plane["type"] | "");
      strncpy(s_tmp[slot].type, ty, sizeof(s_tmp[slot].type) - 1);
      s_tmp[slot].type[sizeof(s_tmp[slot].type) - 1] = '\0';
      // ICAO hex - stabilni identifikator (nemeni se mezi stazenimi).
      const char* hx = plane["hex"] | "";
      strncpy(s_tmp[slot].icao, hx, sizeof(s_tmp[slot].icao) - 1);
      s_tmp[slot].icao[sizeof(s_tmp[slot].icao) - 1] = '\0';
      copyCallsign(&s_tmp[slot], plane);
    }

    // Commitni docasny snimek do ziveho seznamu naraz.
    for (int i = 0; i < n; i++) s_list[i] = s_tmp[i];
    s_count = n;
    if (dropped) Serial.printf("Letadla: %d (%ld bajtu, %d vzdalenych vynechano)\n",
                               n, len, dropped);
    else         Serial.printf("Letadla: %d (%ld bajtu)\n", n, len);
    return true;
  }
  return false;
}
