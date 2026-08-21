// =============================================================================
//  ESPD35_MeteoPlaneRadar - konfiguracni web server. Viz WebConfig.h.
// =============================================================================
#include "WebConfig.h"
#include "WebPage.h"
#include "WiFiPortal.h"   // PORTAL_IP
#include "Settings.h"
#include "Status.h"
#include "Clock.h"
#include "Net.h"
#include "Lang.h"
#include "UI.h"
#include "Version.h"
#include "Config.h"
#include "Watchdog.h"
#include "ADSB.h"
#include "Forecast.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Update.h>          // soucast ESP32 core - nahrada ElegantOTA
#include <esp_heap_caps.h>
#include <esp_system.h>

static WebServer s_srv(WEB_PORT);
static DNSServer s_dns;

static bool s_apMode  = false;
static bool s_running = false;
static bool s_wantConnect = false;
static bool s_wantRestart = false;
static bool s_locChanged  = false;
static bool s_wantFactory = false;
static volatile bool s_updating = false;

// Fronta pozadavku dalkoveho ovladani - viz poznamka ve WebConfig.h.
static uint16_t s_rotSecs      = 0;
static uint32_t s_rotPauseLeft = 0;

static int s_reqScreen     = -1;
static int s_reqScreenStep = 0;
static int s_reqRangeStep  = 0;

void WebConfig_SetRotateInfo(uint16_t secs, uint32_t pauseLeftSec) {
  s_rotSecs = secs;
  s_rotPauseLeft = pauseLeftSec;
}

bool WebConfig_UpdateBusy()       { return s_updating; }
bool WebConfig_WantsWifiConnect() { return s_wantConnect; }
void WebConfig_ClearWifiConnect() { s_wantConnect = false; }
bool WebConfig_WantsRestart()     { return s_wantRestart; }
bool WebConfig_TakeLocationChanged() { bool v = s_locChanged; s_locChanged = false; return v; }
bool WebConfig_WantsFactoryReset(){ return s_wantFactory; }

int WebConfig_TakeScreen()     { int v = s_reqScreen;     s_reqScreen = -1;    return v; }
int WebConfig_TakeScreenStep() { int v = s_reqScreenStep; s_reqScreenStep = 0; return v; }
int WebConfig_TakeRangeStep()  { int v = s_reqRangeStep;  s_reqRangeStep = 0;  return v; }

// --- Pomucky ----------------------------------------------------------------
static void sendJson(int code, JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  s_srv.send(code, "application/json", out);
}

static void sendOk() { s_srv.send(200, "application/json", "{\"ok\":true}"); }

static bool readBody(JsonDocument& doc) {
  if (!s_srv.hasArg("plain")) return false;
  return deserializeJson(doc, s_srv.arg("plain")) == DeserializationError::Ok;
}

// Kazdy destruktivni koncovy bod jde pres tohle. Kdyz heslo nastavene neni,
// pousti to vsechno - to je dokumentovany vychozi stav a deska posloucha jen
// na domaci siti.
static bool authed(JsonDocument& body) {
  const char* pw = body["password"] | "";
  if (Settings_CheckAdminPassword(pw)) return true;
  s_srv.send(403, "application/json", "{\"error\":\"heslo\"}");
  return false;
}

// -----------------------------------------------------------------------------
//  Stranka
// -----------------------------------------------------------------------------
static void handleRoot() {
  s_srv.sendHeader("Cache-Control", "no-store");
  // Znakova sada patri do HLAVICKY, ne jen do meta znacky: prohlizec, ktery
  // dostane text/html bez charset, si kodovani hada - a cesky text s
  // diakritikou pak dopadne spatne.
  s_srv.send(200, "text/html; charset=utf-8", WEB_PAGE_HTML);
}

// -----------------------------------------------------------------------------
//  Konfigurace
// -----------------------------------------------------------------------------
static void handleGetConfig() {
  JsonDocument doc;
  JsonObject o = doc.to<JsonObject>();
  Settings_ToJson(o);

  // Seznamy rozsahu, aby si je stranka nemusela opisovat z Config.h.
  //
  // POZOR NA KRATKE NAZVY VELKYMI PISMENY. Puvodne se tyhle dve pole jmenovaly
  // PR a MR - a `MR` je makro z xtensa/config/specreg.h (`#define MR 32`),
  // ktere sem doputuje pres Arduino.h -> FreeRTOS.h -> portmacro.h. Preklad
  // pak spadl na "expected unqualified-id before numeric constant", coz o
  // skutecne pricine nerika nic. Tenhle hlavickovy soubor obsazuje spoustu
  // dvou- a trimistnych zkratek (BR, PS, SAR, DDR, EPC, MISC...), takze
  // v tomhle projektu POUZIVEJTE POPISNE NAZVY.
  static const float planeRangesKm[] = PLANE_RANGES_KM;
  static const float meteoRangesKm[] = METEO_RANGES_KM;
  JsonArray pr = o["planeRanges"].to<JsonArray>();
  for (unsigned i = 0; i < sizeof(planeRangesKm) / sizeof(planeRangesKm[0]); i++)
    pr.add(planeRangesKm[i]);
  JsonArray mr = o["meteoRanges"].to<JsonArray>();
  for (unsigned i = 0; i < sizeof(meteoRangesKm) / sizeof(meteoRangesKm[0]); i++)
    mr.add(meteoRangesKm[i]);

  o["version"]  = FW_VERSION;
  o["screenN"]  = SCREEN_N;
  sendJson(200, doc);
}

static void handlePostConfig() {
  JsonDocument body;
  if (!readBody(body)) { s_srv.send(400, "application/json", "{\"error\":\"json\"}"); return; }

  // ZADNE nastaveni uz nevyzaduje restart.
  //
  // Do 0.3.0 si o nej rekla zmena polohy a zmena sady obrazovek. Ani jedno to
  // ale nepotrebuje:
  //   - Sada obrazovek: spravce obrazovek v .ino se pta Settings_ScreenEnabled()
  //     pri kazdem prepnuti i pri kresleni tecek, takze zmena plati okamzite.
  //     Jediny problem by nastal, kdyby uzivatel vypnul PRAVE zobrazenou
  //     obrazovku - to vyresi loop() tim, ze prepne na prvni zapnutou.
  //   - Poloha: ADSB i CHMU si ji ctou pri kazdem stazeni. Jen predpoved a
  //     mapa pocasi drzi data pro STAROU polohu, takze staci rict predpovedi,
  //     ze ma zahodit, co ma, a stahnout znovu.
  //
  // Restart tim padem zustava jen jako vedome tlacitko ve Sprave. Uzivatel,
  // kterému se deska restartuje po kazdem ulozeni jasu, prestane nastaveni
  // pouzivat.
  const double lat0 = Settings_Lat(), lon0 = Settings_Lon();

  Settings_FromJson(body.as<JsonObjectConst>());

  if (Settings_Lat() != lat0 || Settings_Lon() != lon0) {
    Forecast_Invalidate();       // stara predpoved patri jine obci
    s_locChanged = true;         // loop() da vedet obrazovkam
  }

  // Jas se ma projevit okamzite, at je videt, co posuvnik dela.
  Backlight_Set(Settings_Backlight());

  sendOk();
}

// -----------------------------------------------------------------------------
//  Stav
// -----------------------------------------------------------------------------
static void handleStatus() {
  JsonDocument doc;
  JsonObject o = doc.to<JsonObject>();

  o["version"] = FW_VERSION;
  o["uptime"]  = (uint32_t)(millis() / 1000);
  o["heapInternal"] = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  o["heapPsram"]    = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

  o["ap"] = s_apMode;
  if (s_apMode) {
    o["ssid"] = AP_SSID;
    o["ip"]   = WiFi.softAPIP().toString();
    o["rssi"] = 0;
  } else {
    o["ssid"] = WiFi.SSID();
    o["ip"]   = WiFi.localIP().toString();
    o["rssi"] = WiFi.RSSI();
  }
  o["host"] = WEB_HOSTNAME;

  o["clockSource"] = Clock_Source();
  o["clockValid"]  = Clock_Valid();
  {
    time_t now = time(nullptr);
    struct tm lt;
    localtime_r(&now, &lt);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", lt.tm_hour, lt.tm_min, lt.tm_sec);
    o["time"] = Clock_Valid() ? buf : "--:--:--";
  }

  o["screen"]        = Settings_Screen();
  o["rotateSec"]     = s_rotSecs;
  o["rotatePauseLeft"] = s_rotPauseLeft;
  o["isNight"] = Settings_IsNight();
  o["planes"]  = ADSB_Count();

  char buf[64];
  JsonObject st = o["sources"].to<JsonObject>();
  Status_Text(ST_ADSB, buf, sizeof(buf));  st["adsb"]  = buf;
  Status_Text(ST_RADAR, buf, sizeof(buf)); st["radar"] = buf;
  Status_Text(ST_FORECAST, buf, sizeof(buf)); st["forecast"] = buf;

  sendJson(200, doc);
}

// -----------------------------------------------------------------------------
//  Dalkove ovladani displeje
// -----------------------------------------------------------------------------
static void handleScreen() {
  JsonDocument body;
  if (!readBody(body)) { s_srv.send(400, "application/json", "{\"error\":\"json\"}"); return; }
  if (body["index"].is<int>()) {
    int idx = body["index"];
    if (idx < 0 || idx >= SCREEN_N) { s_srv.send(400, "application/json", "{\"error\":\"index\"}"); return; }
    if (!Settings_ScreenEnabled((uint8_t)idx)) {
      s_srv.send(409, "application/json", "{\"error\":\"obrazovka je vypnuta\"}");
      return;
    }
    s_reqScreen = idx;
  } else if (body["step"].is<int>()) {
    s_reqScreenStep = ((int)body["step"] < 0) ? -1 : +1;
  }
  sendOk();
}

static void handleRange() {
  JsonDocument body;
  if (!readBody(body)) { s_srv.send(400, "application/json", "{\"error\":\"json\"}"); return; }
  s_reqRangeStep = ((int)(body["step"] | 1) < 0) ? -1 : +1;
  sendOk();
}

// -----------------------------------------------------------------------------
//  WiFi
// -----------------------------------------------------------------------------
static void handleScan() {
  // Sken je synchronni a trva par sekund; watchdog se krmi uvnitr.
  int n = WiFi.scanNetworks(false, false);
  Watchdog_Feed();

  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < n && i < 24; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    o["open"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
  }
  WiFi.scanDelete();
  sendJson(200, doc);
}

static void handleWifi() {
  JsonDocument body;
  if (!readBody(body)) { s_srv.send(400, "application/json", "{\"error\":\"json\"}"); return; }
  const char* ssid = body["ssid"] | "";
  const char* pass = body["pass"] | "";
  if (!ssid[0]) { s_srv.send(400, "application/json", "{\"error\":\"ssid\"}"); return; }

  Settings_SetWifi(ssid, pass);
  // Odpoved se posila HNED a pripojeni se zkusi az z loop(). Kdybychom se
  // pripojovali tady, prohlizec by mezitim ztratil sit pod rukama (deska
  // opousti pristupovy bod) a uzivatel by videl jen chybu nacitani.
  sendOk();
  s_wantConnect = true;
}

// -----------------------------------------------------------------------------
//  Hledani mesta -> souradnice (proxy na Open-Meteo)
//
//  Proxy proto, ze stranka bezi na http:// a Open-Meteo je https:// - prohlizec
//  by to bral jako smiseny obsah a zablokoval. Deska ten pozadavek udela za ni.
// -----------------------------------------------------------------------------
static void handleGeocode() {
  String q = s_srv.arg("q");
  if (q.length() < 2) { s_srv.send(400, "application/json", "{\"error\":\"dotaz\"}"); return; }
  if (s_apMode) {
    // V rezimu pristupoveho bodu neni kudy ven - rict to rovnou je lepsi nez
    // nechat uzivatele cekat na timeout.
    s_srv.send(503, "application/json", "{\"error\":\"bez internetu\"}");
    return;
  }

  String url = String(GEOCODE_URL) + "?name=" + q + "&count=8&language=cs&format=json";
  String resp;
  if (!Net_GetString(url.c_str(), resp, "GEOKOD")) {
    s_srv.send(502, "application/json", "{\"error\":\"nedostupne\"}");
    return;
  }
  s_srv.send(200, "application/json", resp);
}

// -----------------------------------------------------------------------------
//  Zaloha a obnova
// -----------------------------------------------------------------------------
static void handleExport() {
  JsonDocument doc;
  JsonObject o = doc.to<JsonObject>();
  Settings_ToJson(o);
  o["version"] = FW_VERSION;
  // Udaje k WiFi ani heslo v zaloze nejsou - zaloha se posila mailem, uklada
  // do cloudu a prilepuje do issue. Ulozene heslo od domaci site tam nepatri.
  o.remove("wifiSsid");
  o.remove("hasPassword");

  String out;
  serializeJsonPretty(doc, out);
  s_srv.sendHeader("Content-Disposition",
                   "attachment; filename=\"espd35-meteoradar.json\"");
  s_srv.send(200, "application/json", out);
}

static void handleImport() {
  JsonDocument body;
  if (!readBody(body)) { s_srv.send(400, "application/json", "{\"error\":\"json\"}"); return; }
  if (!authed(body)) return;
  Settings_FromJson(body.as<JsonObjectConst>());
  Backlight_Set(Settings_Backlight());
  Forecast_Invalidate();
  s_locChanged = true;
  sendOk();
  // Obnova ze zalohy meni vsechno najednou vcetne polohy, takze tady restart
  // smysl ma - je to jednorazova akce a uzivatel ji cekal.
  s_wantRestart = true;
}

// -----------------------------------------------------------------------------
//  Restart a tovarni reset
// -----------------------------------------------------------------------------
static void handleReboot() {
  JsonDocument body;
  readBody(body);
  if (!authed(body)) return;
  sendOk();
  s_wantRestart = true;
}

static void handleReset() {
  JsonDocument body;
  if (!readBody(body)) { s_srv.send(400, "application/json", "{\"error\":\"json\"}"); return; }
  if (!authed(body)) return;
  sendOk();
  s_wantFactory = true;
}

// -----------------------------------------------------------------------------
//  Aktualizace firmwaru (Update z ESP32 core)
//
//  ElegantOTA je pryc kvuli AGPL-3.0 (viz WebConfig.h). Trida Update, kterou
//  stejne obaloval, je soucast core - nahrada tedy nepridava zadnou zavislost.
//
//  PROGRESS BAR NA DISPLEJI ZUSTAVA. Kreslit z obsluhy HTTP pozadavku je jinak
//  v tomhle projektu zakazane (viz fronty vyse), ale tady plati vyjimka, kterou
//  ma CHANGELOG popsanou uz od 0.3.0: ILI9488 na SPI si obraz drzi ve vlastni
//  pameti a posila mu ho tentyz task, ktery zapisuje flash, takze se ty dve
//  veci nikdy neprekryvaji. (Zdrojovy projekt s RGB panelem ST7701 prubeh
//  kreslit nemuze - jeho panel si obraz prubezne cte z PSRAM a zapis do flash
//  mu data odrezava.)
//
//  Prekresluje se jen po celych dvou procentech: jeden flush je prenos celeho
//  snimku (480*320*2 = 300 kB) po SPI, tedy desitky ms. Kreslit pri kazdem
//  paketu by nahravani vyrazne zpomalilo.
// -----------------------------------------------------------------------------
static size_t s_otaTotal = 0;
static size_t s_otaDone  = 0;
static int    s_otaLastPct = -1;

void WebConfig_DrawOtaProgress(int pct) {
  gfx->fillScreen(C_BLACK);
  UI_TextCentered("Probiha aktualizace", 90, C_WHITE, 2);
  UI_TextCentered("Neodpojujte napajeni", 118, C_YELLOW, 1);

  const int bx = 60, by = 150, bw = LCD_WIDTH - 120, bh = 34;
  gfx->drawRoundRect(bx, by, bw, bh, 6, C_GRAY);
  if (pct >= 0) {
    int fill = (bw - 4) * pct / 100;
    if (fill > 0) gfx->fillRoundRect(bx + 2, by + 2, fill, bh - 4, 4, C_GREEN);
    char t[8];
    snprintf(t, sizeof(t), "%d%%", pct);
    UI_TextCentered(t, by + bh + 16, C_WHITE, 2);
  } else {
    // Velikost neznama (prohlizec ji neposlal) - procenta by lhala.
    UI_TextCentered("...", by + bh + 16, C_WHITE, 2);
  }
  UI_TextCentered("Po dokonceni se deska sama restartuje.", 250, C_GRAY, 1);
  gfx->flush();
}

static void otaFinish() {
  const bool ok = !Update.hasError();
  s_srv.sendHeader("Connection", "close");
  s_srv.send(ok ? 200 : 500, "application/json",
             ok ? "{\"ok\":true}" : "{\"ok\":false}");

  gfx->fillScreen(C_BLACK);
  UI_TextCentered(ok ? "Hotovo, restartuji..." : "Aktualizace selhala",
                  LCD_HEIGHT / 2 - 10, ok ? C_GREEN : C_RED, 2);
  if (!ok) UI_TextCentered("Puvodni verze zustava v desce.",
                           LCD_HEIGHT / 2 + 20, C_GRAY, 1);
  gfx->flush();

  if (ok) {
    Serial.println("OTA: hotovo, restartuji");
    delay(600);
    ESP.restart();
  }
  delay(2000);
  s_updating = false;
}

static void otaUpload() {
  HTTPUpload& up = s_srv.upload();

  if (up.status == UPLOAD_FILE_START) {
    if (Settings_HasAdminPassword() &&
        !s_srv.authenticate(WEB_ADMIN_USER, Settings_AdminPassword())) {
      s_srv.requestAuthentication();
      return;
    }
    // Presnou velikost posila prohlizec v dotazu (?size=...). Bez ni by
    // Update.size() vratilo velikost celeho SLOTU (6 MB) a ukazatel prubehu
    // by skoncil nekde na dvaceti procentech.
    s_otaTotal = (size_t)s_srv.arg("size").toInt();
    s_otaDone  = 0;
    s_otaLastPct = -1;
    s_updating = true;              // loop() prestane kreslit i stahovat
    Watchdog_Feed();
    Serial.printf("OTA: start, %s (%u B)\n",
                  up.filename.c_str(), (unsigned)s_otaTotal);
    if (!Update.begin(s_otaTotal ? s_otaTotal : UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
      s_updating = false;
      return;
    }
    WebConfig_DrawOtaProgress(s_otaTotal ? 0 : -1);
  }
  else if (up.status == UPLOAD_FILE_WRITE) {
    if (Update.write(up.buf, up.currentSize) != up.currentSize) Update.printError(Serial);
    s_otaDone += up.currentSize;
    Watchdog_Feed();
    if (s_otaTotal) {
      int pct = (int)((uint64_t)s_otaDone * 100 / s_otaTotal);
      if (pct > 100) pct = 100;
      if (pct / 2 != s_otaLastPct / 2) {   // jen po celych 2 % - viz hlavicka
        s_otaLastPct = pct;
        WebConfig_DrawOtaProgress(pct);
      }
    }
  }
  else if (up.status == UPLOAD_FILE_END) {
    if (!Update.end(true)) { Update.printError(Serial); s_updating = false; }
  }
  else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    s_updating = false;
    Serial.println("OTA: preruseno");
  }
}

// -----------------------------------------------------------------------------
//  Captive portal
// -----------------------------------------------------------------------------
static void handleNotFound() {
  if (s_apMode) {
    // Cokoli jineho presmerovat na nasi stranku - telefon si tim vsimne, ze
    // sit "chce prihlaseni", a nabidne ji sam.
    s_srv.sendHeader("Location", String("http://") + PORTAL_IP + "/", true);
    s_srv.send(302, "text/plain", "");
    return;
  }
  s_srv.send(404, "text/plain", "404");
}

// -----------------------------------------------------------------------------
//  Start a smycka
// -----------------------------------------------------------------------------
void WebConfig_Begin(bool apMode) {
  if (s_running) { s_srv.stop(); s_dns.stop(); MDNS.end(); s_running = false; }
  s_apMode = apMode;

  s_srv.on("/", HTTP_GET, handleRoot);
  s_srv.on("/api/config", HTTP_GET,  handleGetConfig);
  s_srv.on("/api/config", HTTP_POST, handlePostConfig);
  s_srv.on("/api/status", HTTP_GET,  handleStatus);
  s_srv.on("/api/screen", HTTP_POST, handleScreen);
  s_srv.on("/api/range",  HTTP_POST, handleRange);
  s_srv.on("/api/scan",   HTTP_GET,  handleScan);
  s_srv.on("/api/wifi",   HTTP_POST, handleWifi);
  s_srv.on("/api/geocode",HTTP_GET,  handleGeocode);
  s_srv.on("/api/export", HTTP_GET,  handleExport);
  s_srv.on("/api/import", HTTP_POST, handleImport);
  s_srv.on("/api/reboot", HTTP_POST, handleReboot);
  s_srv.on("/api/reset",  HTTP_POST, handleReset);
  // Druhy handler bezi po kouscich behem prijmu, prvni az na konci.
  s_srv.on("/update", HTTP_POST, otaFinish, otaUpload);
  s_srv.onNotFound(handleNotFound);

  s_srv.begin();
  s_running = true;

  if (apMode) {
    s_dns.start(53, "*", WiFi.softAPIP());
    Serial.printf("Web: captive portal na http://%s/\n", PORTAL_IP);
  } else {
    if (MDNS.begin(WEB_HOSTNAME)) {
      MDNS.addService("http", "tcp", WEB_PORT);
      Serial.printf("Web: http://%s.local/ (i http://%s/)\n",
                    WEB_HOSTNAME, WiFi.localIP().toString().c_str());
    } else {
      Serial.printf("Web: http://%s/ (mDNS se nespustil)\n",
                    WiFi.localIP().toString().c_str());
    }
  }
}

void WebConfig_Loop() {
  if (!s_running) return;
  if (s_apMode) s_dns.processNextRequest();
  s_srv.handleClient();
}
