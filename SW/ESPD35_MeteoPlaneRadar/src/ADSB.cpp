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
#include <stdlib.h>          // strtof
#include "esp_heap_caps.h"   // PSRAM body buffer
#include "NetSink.h"
#include "PlaneIcon.h"         // cteni tela bezpecne vuci chunked
#include "Net.h"        // strazce interni pameti + hlavicka Date
#include "Status.h"     // jednoradkove hlaseni pro stavovou stranku
#include "Settings.h"   // vyskove pasmo, "jen s volacim znakem"
#include "Config.h"     // SQUAWK_*

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

// Kolik letadel odfiltrovalo NASTAVENI (ne vzdalenost). Drzi se zvlast, aby
// slo na stavove strance odlisit "nad vami nic nelita" od "mate prisny filtr".
static int s_filteredOut = 0;

void ADSB_SetPollFn(void (*fn)()) { s_poll = fn; }
int  ADSB_Count() { return s_count; }
const Aircraft* ADSB_List() { return s_list; }

int ADSB_FindByHex(const char* hex) {
  if (!hex || !hex[0]) return -1;
  for (int i = 0; i < s_count; i++) {
    if (strcmp(s_list[i].hex, hex) == 0) return i;
  }
  return -1;   // uz neni v datech
}

int ADSB_FilteredOut() { return s_filteredOut; }

const char* ADSB_EmergencyCode(const Aircraft& a) {
  if (!a.squawk[0]) return nullptr;
  if (strcmp(a.squawk, SQUAWK_HIJACK) == 0) return SQUAWK_HIJACK;
  if (strcmp(a.squawk, SQUAWK_RADIO)  == 0) return SQUAWK_RADIO;
  if (strcmp(a.squawk, SQUAWK_EMERG)  == 0) return SQUAWK_EMERG;
  return nullptr;
}

// Projde letadlo uzivatelskym filtrem? Nouzovy squawk filtr PRESKAKUJE -
// letadlo hlasici 7700 je presne to, co chcete videt, i kdyz je zrovna mimo
// nastavene vyskove pasmo.
static bool passesFilter(const Aircraft& a) {
  if (Settings_SquawkAlert() && ADSB_EmergencyCode(a)) return true;
  if (Settings_OnlyWithCallsign() && !a.callsign[0]) return false;
  const uint16_t lo = Settings_AltMinFt();
  const uint16_t hi = Settings_AltMaxFt();
  // Letadlo, ktere vysku vubec nehlasi (altFt == 0), se pasmem neposuzuje -
  // jinak by ho spodni mez vyhodila, aniz by o nem cokoli bylo znamo.
  if (a.altFt > 0.0f) {
    if (a.altFt < (float)lo) return false;
    if (a.altFt > (float)hi) return false;
  }
  return true;
}

static void poll() { if (s_poll) s_poll(); }

// Cte cislo i kdyz je v JSON jako retezec.
static bool readFloat(JsonObjectConst o, const char* key, float* out) {
  JsonVariantConst v = o[key];
  if (v.is<float>() || v.is<double>() || v.is<int>()) { *out = v.as<float>(); return true; }
  if (v.is<const char*>()) {
    // ArduinoJson si ciselny retezec prevede sam, jenze atof() nerozlisi
    // chybejici hodnotu od necislne: z "ground" udelal 0.0 a ohlasil uspech.
    // Koncovy ukazatel u strtof() ten rozdil ukaze, takze necekany literal
    // propadne misto toho, aby se z nej stala nulova vyska.
    const char* str = v.as<const char*>();
    if (!str || !*str) return false;
    char* end = nullptr;
    float f = strtof(str, &end);
    if (end == str) return false;
    *out = f;
    return true;
  }
  return false;
}

// Volacka, a JEN volacka. Drive tu byl zaloznik na hex adresu, aby letadlo,
// ktere volacku nevysila (TIS-B, MLAT, soukrome a vojenske stroje), aspon neco
// ukazalo. To byla chyba: volacka jde do dotazu na trasu a hex adresa se po
// normalizaci tvari jako platne cislo letu - "a31234" se zmeni na "A31234",
// coz je Aegean Airlines 1234, takze letadlo nad Prahou hlasilo let
// Atheny - Istanbul. Bez volacky zustava pole prazdne a na trasu se nikdo
// nepta; kdo potrebuje neco vykreslit, sahne po hexu sam (viz ScreenPlanes).
static void copyCallsign(Aircraft* a, JsonObjectConst plane) {
  const char* src = plane["flight"] | "";
  while (*src == ' ' || *src == '\t') src++;   // adsb.fi doplnuje na osm znaku
  int i = 0;
  while (src[i] && i < (int)sizeof(a->callsign) - 1) { a->callsign[i] = src[i]; i++; }
  while (i > 0 && (a->callsign[i-1] == ' ' || a->callsign[i-1] == '\t')) i--;
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
// Rezervuje se, kdyz server neposle Content-Length (chunked). Pod tvrdym
// stropem, nad vsim, co adsb.fi realne vrati: nejsirsi nabizeny dosah je
// 100 km, kde odpoved ma nizke desitky kB.
static const size_t ADSB_UNKNOWN_BODY = 384 * 1024;

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
// chybe: alokace, preteceni, zaseknuti nebo prenos ukonceny driv, nez slibovala
// deklarovana delka.
static long readBody(HTTPClient& http) {
  // writeToStream() dekoduje chunked; rucni smycka nad getStreamPtr() ne, a
  // presne proto zustavaly velikosti bloku v tele.
  int declared = http.getSize();               // -1 kdyz chunked / neznamy
  if (declared > (int)ADSB_MAX_BODY) {
    Serial.printf("ADSB: hlaseno %d B, strop je %u B\n",
                  declared, (unsigned)ADSB_MAX_BODY);
    return -1;
  }
  // Sink zapisuje do pevneho bufferu a neumi ho zvetsit uprostred prenosu,
  // takze chunked odpoved musi dostat misto dopredu. Buffer lezi v PSRAM a
  // recykluje se mezi stazenimi, takze ta rezerva nic nestoji.
  size_t want = (declared > 0) ? (size_t)declared + 1 : ADSB_UNKNOWN_BODY;
  if (!bodyReserve(want)) return -1;

  return Net_ReadBody(http, (uint8_t*)s_body, s_bodyCap, "ADSB", s_poll);
}

// Filtr: parsuji se jen klice, ktere pouzivame, takze JsonDocument zustava maly
// bez ohledu na to, kolik adsb.fi posle. alt_baro MUSI zustat - nese literal
// "ground" pro detekci letadla na zemi.
static void fillFields(JsonObject o) {
  if (o.isNull()) return;
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
  o["r"]            = true;   // registrace - zadarmo v teze odpovedi
  o["squawk"]       = true;
  o["category"]     = true;   // kategorie vysilace - ikona, kdyz typ chybi
}

static void buildFilter(JsonDocument& filter) {
  // Pole letadel muze prijit pod dvema jmeny: adsb.fi a adsb.lol ho posilaji
  // jako "ac", servery odvozene z ADSBexchange (tar1090 / readsb) jako
  // "aircraft". Filtr pusti obe, jinak by druhy tvar vysel jako prazdny
  // dokument a radar by zustal tise prazdny.
  //
  // Kazdy obsah se vyplni pres vlastni JsonObject z add<>() - pridani dalsiho
  // klice do filtru muze zneplatnit JsonObject vzaty driv (ArduinoJson 7).
  fillFields(filter["ac"].add<JsonObject>());
  fillFields(filter["aircraft"].add<JsonObject>());

  // Neni to uzitecny obsah, ale diagnostika: adsb.fi dava svuj stavovy text do
  // "msg" ("No error" pri uspechu). Filtr ho driv zahazoval, takze firmware
  // umel rict jen "chybi pole ac" a nic o tom proc. Prida se az nakonec.
  filter["msg"] = true;
}

// --- Stari dat a pauza po chybach (0.8.4) ------------------------------------
static bool          s_everOk     = false;
static unsigned long s_lastOkMs   = 0;
static unsigned long s_blockUntil = 0;
static bool          s_blocked    = false;
static uint8_t       s_failCount  = 0;
static int           s_lastCode   = 0;

unsigned long ADSB_DataAgeMs() { return s_everOk ? millis() - s_lastOkMs : 0xFFFFFFFFUL; }
bool ADSB_DataStale()  { return ADSB_DataAgeMs() > ADSB_STALE_MS; }
bool ADSB_DataHidden() { return ADSB_DataAgeMs() > ADSB_HIDE_MS; }
int  ADSB_LastHttpCode() { return s_lastCode; }

unsigned long ADSB_PauseLeftMs() {
  if (!s_blocked) return 0;
  const long left = (long)(s_blockUntil - millis());
  if (left <= 0) { s_blocked = false; return 0; }
  return (unsigned long)left;
}

// Kazda dalsi chyba v rade pauzu zdvojnasobi. Retry-After od serveru ma
// prednost, kdyz je delsi.
static void noteFailure(long retryAfterS) {
  if (s_failCount < 10) s_failCount++;
  unsigned long pause = ADSB_BACKOFF_BASE_MS << (s_failCount - 1);
  if (pause > ADSB_BACKOFF_MAX_MS) pause = ADSB_BACKOFF_MAX_MS;
  if (retryAfterS > 0) {
    if (retryAfterS > ADSB_RETRY_AFTER_MAX_S) retryAfterS = ADSB_RETRY_AFTER_MAX_S;
    if ((unsigned long)retryAfterS * 1000UL > pause) pause = (unsigned long)retryAfterS * 1000UL;
  }
  s_blockUntil = millis() + pause;
  s_blocked = true;
  Serial.printf("ADSB: chyba %d, %u. v rade, pauza %lu s\n",
                s_lastCode, (unsigned)s_failCount, pause / 1000UL);
  Status_Set(ST_ADSB, "chyba %d, pauza %lu s", s_lastCode, pause / 1000UL);
}

static void noteSuccess() {
  s_everOk = true;
  s_lastOkMs = millis();
  s_failCount = 0;
  s_blocked = false;
}

bool ADSB_Fetch(double lat, double lon, float radiusKm) {
  // Bez pripojeni -> nech, co je na obrazovce (radar NEVYMAZAT).
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("ADSB: no WiFi");
    Status_Set(ST_ADSB, "bez WiFi");
    return false;
  }
  // TLS handshake potrebuje ~45 kB interni RAM; bez ni selze uvnitr mbedTLS
  // a navenek to vypada jako hole "HTTP -1". Radeji poll preskocit.
  if (!Net_HeapOk("ADSB")) { Status_Set(ST_ADSB, "malo pameti"); return false; }
  // Pauza po chybach - neblokuje, jen se tentokrat neptat.
  if (ADSB_PauseLeftMs() > 0) return false;
  long retryAfter = 0;

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
    client.setHandshakeTimeout(NET_TLS_HANDSHAKE_S);

    HTTPClient http;
    http.setConnectTimeout(8000);   // ms - jen TCP connect, NE handshake
    http.setTimeout(12000);         // ms - cteni
    http.setReuse(false);
    if (!http.begin(client, url)) {
      Serial.println("ADSB: begin failed");
      if (attempt < MAX_ATTEMPTS) { delay(200); continue; }
      noteFailure(0);
      return false;
    }
    // Slusne se predstavit - bezplatne API adsb.fi o to zada.
    // POZOR: setUserAgent(), NE addHeader(). ESP32 HTTPClient::addHeader()
    // mlcky zahazuje Connection, Host, Accept-Encoding a prave User-Agent -
    // vidi je jako "handled by code" a nic nenahlasi. Tahle hlavicka se tedy
    // driv neposilala vubec a odchazela vychozi "ESP32HTTPClient"; adsb.lol na
    // ni odpovida 403 "User-Agent too generic". Jeden retezec pro obe API je
    // v Config.h.
    http.setUserAgent(HTTP_USER_AGENT);
    http.addHeader("Accept", "application/json");
    // Hlavicka Date je zaloha hodin, kdyz NTP neprojde (viz Clock.h).
    static const char* WANTED[] = { "Date", "Retry-After" };
    http.collectHeaders(WANTED, 2);

    int code = http.GET();
    s_lastCode = code > 0 ? code : 0;
    if (code != HTTP_CODE_OK) {
      Serial.printf("ADSB: HTTP %d (pokus %d)\n", code, attempt);
      if (http.hasHeader("Retry-After")) retryAfter = http.header("Retry-After").toInt();
      http.end();
      // Pretizeny server (429, 5xx) se v temze pokusu znovu nezkousi.
      const bool overloaded = (code == 429 || code >= 500);
      if (!overloaded && attempt < MAX_ATTEMPTS) { delay(200); continue; }
      noteFailure(retryAfter);
      return false;   // nech posledni dobra data
    }
    Net_NoteDate(http);

    // Nacti CELE telo (do PSRAM), pak teprve parsuj - zadny parse ze streamu.
    long len = readBody(http);
    http.end();

    if (len < 0) {
      Serial.println("ADSB: cteni tela selhalo");
      if (attempt < MAX_ATTEMPTS) { delay(200); continue; }
      noteFailure(0);
      return false;
    }
    if (len < 8) {
      Serial.printf("ADSB: kratke telo %ld (pokus %d)\n", len, attempt);
      if (attempt < MAX_ATTEMPTS) { delay(200); continue; }
      noteFailure(0);
      return false;
    }

    // Levna kontrola tvaru driv, nez to uvidi parser. Chyti chybovou stranku
    // z proxy, gzip nebo zbytky bloku - tedy vsechno, co by jinak doslo az k
    // ArduinoJsonu a vratilo se jako neco nesrozumitelneho.
    const char* head = s_body;
    while (*head == ' ' || *head == '\r' || *head == '\n' || *head == '\t') head++;
    if (*head != '{') {
      Serial.printf("ADSB: odpoved nezacina JSON objektem, telo[0..120]: %.120s\n", s_body);
      noteFailure(0);
      Status_Set(ST_ADSB, "neocekavana odpoved");
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
      noteFailure(0);
      return false;
    }

    // "ac" (adsb.fi, adsb.lol) nebo "aircraft" (tar1090 / readsb).
    JsonArrayConst ac = doc["ac"].as<JsonArrayConst>();
    if (ac.isNull()) ac = doc["aircraft"].as<JsonArrayConst>();
    if (ac.isNull()) {
      // Rict, CO prislo, ne jen ze to bylo spatne. "msg" je stavovy text
      // serveru, zacatek tela chyti odpovedi, ktere ocekavany tvar nemely
      // nikdy.
      Serial.printf("ADSB: chybi pole letadel - msg: %s\n", doc["msg"] | "(zadne msg)");
      Serial.printf("ADSB: telo[0..200]: %.200s\n", s_body);
      noteFailure(0);
      Status_Set(ST_ADSB, "spatny tvar odpovedi");
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

    int   n = 0;
    int   worstIdx = -1;      // slot s nejvetsi vzdalenosti (jen kdyz je plno)
    float worstD2  = -1.0f;
    int   dropped  = 0;       // kolik letadel se nevešlo (jen pro vypis)

    s_filteredOut = 0;

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

      // Letadlo se nejdriv poskladá CELE do docasne promenne. Az pak jde na
      // radu filtr a teprve potom se pro nej hleda misto v poli - jinak by
      // odfiltrovane letadlo stihlo zabrat slot nekomu, kdo se ma zobrazit.
      Aircraft cand;
      cand.lat = plat;
      cand.lon = plon;
      cand.onGround = false;

      // Smer letu - poznamename, jestli vubec existuje.
      float tr = 0;
      if (readFloat(plane, "track", &tr) || readFloat(plane, "true_heading", &tr)) {
        cand.track = tr;
        cand.hasTrack = true;
      } else {
        cand.track = 0;
        cand.hasTrack = false;
      }
      // Vyska (baro), rychlost, stoupani.
      float f = 0;
      cand.altFt    = readFloat(plane, "alt_baro", &f) ? f : 0;
      cand.gsKt     = readFloat(plane, "gs", &f) ? f : 0;
      cand.baroRate = readFloat(plane, "baro_rate", &f) ? f : 0;
      // Typ letadla. VYHRADNE "t" - to je kod draku ("A320"). Klic "type" je
      // ZDROJ ZPRAVY ("adsb_icao", "mlat", "tisb_icao") a jako zaloha to bylo
      // spatne: letadla, u kterych adsb.fi drak nezna, ukazovala v detailu
      // "adsb_icao" misto typu. Radsi prazdno nez nesmysl - radek se pak
      // proste nevykresli.
      const char* ty = plane["t"] | "";
      strncpy(cand.type, ty, sizeof(cand.type) - 1);
      cand.type[sizeof(cand.type) - 1] = '\0';
      // Registrace, stejna vec - "r" prijde v teze odpovedi, takze na "OK-TVU"
      // vedle typu neni potreba zadne druhe API.
      const char* rg = plane["r"] | "";
      strncpy(cand.reg, rg, sizeof(cand.reg) - 1);
      cand.reg[sizeof(cand.reg) - 1] = '\0';
      // ICAO hex - stabilni identifikator (nemeni se mezi stazenimi).
      const char* hx = plane["hex"] | "";
      strncpy(cand.hex, hx, sizeof(cand.hex) - 1);
      cand.hex[sizeof(cand.hex) - 1] = '\0';
      // Kod odpovidace. Jen ctyri osmickove cislice; cokoli jineho zahodime,
      // aby se do porovnani s 7500/7600/7700 nedostal zmetek.
      //
      // Nektere servery ho posilaji jako CISLO misto retezce (7700 bez
      // uvozovek). `| ""` by z nej udelalo prazdny retezec a nouzovy kod by
      // propadl - proto se cislo prevede zpet na ctyri cislice.
      {
        char sqBuf[8] = "";
        JsonVariantConst sqv = plane["squawk"];
        if (sqv.is<const char*>()) {
          const char* s0 = sqv.as<const char*>();
          if (s0) { strncpy(sqBuf, s0, sizeof(sqBuf) - 1); sqBuf[sizeof(sqBuf) - 1] = '\0'; }
        } else if (sqv.is<int>()) {
          const int v = sqv.as<int>();
          if (v >= 0 && v <= 7777) snprintf(sqBuf, sizeof(sqBuf), "%04d", v);
        }
        bool ok = (strlen(sqBuf) == 4);
        for (int k = 0; ok && k < 4; k++) ok = (sqBuf[k] >= '0' && sqBuf[k] <= '7');
        if (ok) {
          strncpy(cand.squawk, sqBuf, sizeof(cand.squawk) - 1);
          cand.squawk[sizeof(cand.squawk) - 1] = '\0';
        }
      }
      copyCallsign(&cand, plane);

      // Kategorie vysilace a z ni (nebo z typu) ikona - jednou tady, ne pri
      // kazdem prekresleni.
      {
        const char* cat = plane["category"] | "";
        strncpy(cand.cat, cat, sizeof(cand.cat) - 1);
        cand.cat[sizeof(cand.cat) - 1] = '\0';
      }
      cand.icon = PlaneIcon_Classify(cand.type, cand.cat);

      // Uzivatelsky filtr (vyskove pasmo, jen s volacim znakem).
      if (!passesFilter(cand)) { s_filteredOut++; continue; }

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
      s_tmp[slot] = cand;
    }

    // Commitni docasny snimek do ziveho seznamu naraz.
    for (int i = 0; i < n; i++) s_list[i] = s_tmp[i];
    s_count = n;
    noteSuccess();
    if (dropped) Serial.printf("Letadla: %d (%ld bajtu, %d vzdalenych vynechano)\n",
                               n, len, dropped);
    else         Serial.printf("Letadla: %d (%ld bajtu)\n", n, len);
    if (s_filteredOut) Serial.printf("Letadla: %d odfiltrovano nastavenim\n", s_filteredOut);

    if (s_filteredOut) Status_Set(ST_ADSB, "OK, %d letadel (%d filtrem)", n, s_filteredOut);
    else               Status_Set(ST_ADSB, "OK, %d letadel", n);
    return true;
  }
  noteFailure(retryAfter);
  Status_Set(ST_ADSB, "stahovani selhalo");
  return false;
}
