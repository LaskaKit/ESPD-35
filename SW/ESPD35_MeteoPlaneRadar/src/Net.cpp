// =============================================================================
//  ESPD35_MeteoPlaneRadar - sdilene stahovani pres HTTPS. Viz Net.h.
// =============================================================================
#include "Net.h"
#include "Config.h"
#include "Clock.h"
#include "NetSink.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <string.h>

// Drzeno otevrene po dobu davky pozadavku - viz Net.h.
static WiFiClientSecure* s_sess = nullptr;

static void (*s_poll)() = nullptr;
void Net_SetPollFn(void (*fn)()) { s_poll = fn; }
static inline void poll() { if (s_poll) s_poll(); }

const char* NET_DATE_HEADER[1] = { "Date" };

// TLS handshake alokuje zhruba 45 kB INTERNI RAM (PSRAM se na to pouzit neda).
// Kdyz ta alokace selze, mbedTLS to ohlasi jako hole "HTTP -1" bez naznaku
// skutecne priciny. Kontrola predem z toho udela citelny radek v logu.
bool Net_HeapOk(const char* tag) {
  size_t freeInt = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (freeInt >= NET_MIN_HEAP) return true;
  Serial.printf("%s: malo volne interni pameti (%u B < %u B), stahovani preskoceno\n",
                tag, (unsigned)freeInt, (unsigned)NET_MIN_HEAP);
  return false;
}

void Net_NoteDate(HTTPClient& http) {
  if (http.hasHeader("Date")) Clock_NoteHttpDate(http.header("Date").c_str());
}

void Net_SessionBegin() {
  if (s_sess) return;
  s_sess = new WiFiClientSecure();
  if (s_sess) { s_sess->setInsecure();
                s_sess->setHandshakeTimeout(NET_TLS_HANDSHAKE_S); }
}

void Net_SessionEnd() {
  if (!s_sess) return;
  s_sess->stop();
  delete s_sess;
  s_sess = nullptr;
}

// Sdileny buffer na textova tela (PSRAM, recykluje se mezi volanimi). Roste na
// velikost, kterou server ohlasi, a nikdy pres NET_MAX_TEXT. Drive telo bral
// http.getString() bez stropu a na interni heap, ze ktereho si bere i TLS.
static char*  s_txt    = nullptr;
static size_t s_txtCap = 0;

static bool txtReserve(size_t need) {
  if (need <= s_txtCap) return true;
  char* nb = (char*)heap_caps_realloc(s_txt, need, MALLOC_CAP_SPIRAM);
  if (!nb) { Serial.println("NET: telo se nevejde do PSRAM"); return false; }
  s_txt = nb; s_txtCap = need;
  return true;
}

bool Net_GetString(const char* url, String& out, const char* tag) {
  out = "";
  if (WiFi.status() != WL_CONNECTED) { Serial.printf("%s: bez WiFi\n", tag); return false; }

  // I obycejne http:// sem patri - ip-api.com dava TLS jen platicim. Drive mel
  // GeoIP kvuli tomu vlastni HTTPClient (a vlastni getString() bez stropu).
  // Strazce pameti hlida handshake, takze plati jen tam, kde nejaky bude, a ne
  // uvnitr otevrene relace.
  const bool tls  = (strncmp(url, "http://", 7) != 0);
  const bool sess = tls && (s_sess != nullptr);
  if (tls && !sess && !Net_HeapOk(tag)) return false;

  WiFiClient       plain;
  WiFiClientSecure own;
  if (tls && !sess) { own.setInsecure(); own.setHandshakeTimeout(NET_TLS_HANDSHAKE_S); }
  WiFiClient& client = !tls ? plain
                     : sess ? static_cast<WiFiClient&>(*s_sess)
                            : static_cast<WiFiClient&>(own);

  HTTPClient http;
  http.setConnectTimeout(8000);   // jen TCP connect, NE handshake
  http.setTimeout(12000);
  http.setReuse(sess);
  if (!http.begin(client, url)) { Serial.printf("%s: begin() selhalo\n", tag); return false; }
  http.setUserAgent(HTTP_USER_AGENT);
  http.collectHeaders(NET_DATE_HEADER, 1);

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("%s: HTTP %d\n", tag, code);
    // U skutecne HTTP odpovedi (ne chyby spojeni) rict, na co se ptalo a co
    // server odpovedel - samotne "HTTP 404" nerekne, jestli je spatne URL,
    // nebo jen zdroj nema data.
    if (code > 0) {
      Serial.printf("%s: URL %s\n", tag, url);
      String err = http.getString();
      if (err.length() > 240) err = err.substring(0, 240);
      Serial.printf("%s: odpoved: %s\n", tag, err.c_str());
    }
    http.end();
    return false;
  }
  Net_NoteDate(http);

  const int declared = http.getSize();          // -1 = chunked / neznamy
  if (declared > (int)NET_MAX_TEXT) {
    Serial.printf("%s: odpoved %d B, strop je %u B\n", tag, declared, (unsigned)NET_MAX_TEXT);
    http.end();
    return false;
  }
  if (!txtReserve((declared > 0) ? (size_t)declared + 1 : NET_MAX_TEXT)) {
    http.end();
    return false;
  }

  // Telo cte sink pres writeToStream(), ktery chunked dekoduje a hlida casovy
  // limit i strop - stejna cesta jako u binarnich dat a u ADSB.
  poll();
  const long len = Net_ReadBody(http, (uint8_t*)s_txt, s_txtCap, tag, s_poll);
  http.end();
  poll();

  if (len <= 0) { Serial.printf("%s: prazdna odpoved\n", tag); return false; }
  out = s_txt;                    // Net_ReadBody telo ukoncil nulou
  return true;
}

bool Net_GetBinary(const char* url, uint8_t* buf, size_t cap, size_t* outLen,
                   const char* tag) {
  if (outLen) *outLen = 0;
  if (!buf || cap == 0) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  // Uvnitr relace uz spojeni stoji, takze velka alokace na handshake nehrozi
  // a strazce pameti by jen prekazel. Mimo relaci plati dal.
  const bool sess = (s_sess != nullptr);
  if (!sess && !Net_HeapOk(tag)) return false;

  WiFiClientSecure  own;
  WiFiClientSecure& client = sess ? *s_sess : own;
  if (!sess) { own.setInsecure(); own.setHandshakeTimeout(NET_TLS_HANDSHAKE_S); }

  HTTPClient http;
  http.setConnectTimeout(8000);   // jen TCP connect, NE handshake
  http.setTimeout(15000);
  http.setReuse(sess);            // nechat socket otevreny pro dalsi snimek
  if (!http.begin(client, url)) return false;
  http.collectHeaders(NET_DATE_HEADER, 1);

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("%s: HTTP %d\n", tag, code);
    http.end();
    return false;
  }
  Net_NoteDate(http);

  int declared = http.getSize();      // -1 = neznamy / chunked
  if (declared > 0 && (size_t)declared > cap) {
    Serial.printf("%s: odpoved %d B se nevejde do %u B\n", tag, declared, (unsigned)cap);
    http.end();
    return false;
  }

  // Telo cte sink pres writeToStream(), ktery chunked dekoduje. Rucni smycka
  // nad getStreamPtr() to neumela a hexadecimalni velikosti bloku koncily
  // primo v obrazku. Neuplny prenos hlasi jako chybu uz HTTPClient sam
  // (porovnava zapsane bajty s Content-Length), takze se sem uriznuty PNG
  // nedostane.
  long got = Net_ReadBody(http, buf, cap, tag, s_poll);
  http.end();

  if (got < 100) return false;        // prazdna nebo chybova odpoved
  if (outLen) *outLen = (size_t)got;
  return true;
}
