// =============================================================================
//  ESPD35_MeteoPlaneRadar - WiFi. Viz WiFiPortal.h.
// =============================================================================
#include "Lang.h"     // LT(): cesky / slovensky, s diakritikou i bez
#include "WiFiPortal.h"
#include "Settings.h"
#include "WebConfig.h"
#include "Lang.h"
#include "UI.h"
#include "Watchdog.h"
#include "Config.h"
#include "Version.h"

#include <WiFi.h>

static bool          s_ap = false;
static unsigned long s_lastRetry = 0;
static unsigned long s_downSince = 0;     // kdy spojeni spadlo (0 = je nahore)
static unsigned long s_apIdleSince = 0;   // od kdy na pristupovem bodu nikdo neni

// Cekani na pripojeni. Neni to "blokujici portal" jako driv - je to jeden
// pokus s pevnym stropem, behem ktereho se krmi watchdog. Kdyz nevyjde,
// vyskoci pristupovy bod a smycka se rozjede normalne.
static bool waitForConnect(unsigned long timeoutMs) {
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (WiFi.status() == WL_CONNECTED) return true;
    // WL_CONNECT_FAILED znamena spatne heslo - cekat dal nema smysl.
    if (WiFi.status() == WL_CONNECT_FAILED) return false;
    Watchdog_Feed();
    delay(100);
  }
  return false;
}

static void startAP() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  const char* pass = (strlen(AP_PASSWORD) == 0) ? nullptr : AP_PASSWORD;
  WiFi.softAP(AP_SSID, pass);
  s_ap = true;
  s_apIdleSince = millis();
  Serial.printf("WiFi: pristupovy bod %s, adresa %s\n", AP_SSID, PORTAL_IP);
  WebConfig_Begin(true);
}

static bool tryConnect(const char* ssid, const char* pass, unsigned long timeoutMs = WIFI_CONNECT_TIMEOUT_MS) {
  Serial.printf("WiFi: pripojuji k %s\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);          // spici radio pridava desitky ms ke kazdemu GET
  WiFi.begin(ssid, pass);
  if (!waitForConnect(timeoutMs)) {
    Serial.println("WiFi: pripojeni se nezdarilo");
    return false;
  }
  Serial.printf("WiFi ok, IP %s\n", WiFi.localIP().toString().c_str());
  // Webovy server v rezimu klienta uz bezi (prepnuti na jinou ulozenou sit) -
  // znovu ho nespoustet, kazdy start by pridal dalsi sadu handleru.
  static bool s_webSta = false;
  if (s_ap || !s_webSta) { WebConfig_Begin(false); s_webSta = true; }
  s_ap = false;
  return true;
}

// Projde ulozene site (0.8.0, az WIFI_SLOTS): nejdriv ty, ktere sken vidi,
// od nejsilnejsi, pak ostatni - skrytou sit sken neuvidi, a presto muze byt
// v dosahu.
static bool connectAny(unsigned long perNetMs) {
  int  order[WIFI_SLOTS];
  int  rssi[WIFI_SLOTS];
  bool seen[WIFI_SLOTS];
  int  n = 0;

  WiFi.mode(WIFI_STA);
  const int found = WiFi.scanNetworks(false, true);
  for (int s = 0; s < WIFI_SLOTS; s++) {
    rssi[s] = -1000;
    seen[s] = false;
    if (!Settings_WifiSsid(s)[0]) continue;
    for (int i = 0; i < found; i++) {
      if (WiFi.SSID(i) == Settings_WifiSsid(s) && WiFi.RSSI(i) > rssi[s]) {
        rssi[s] = WiFi.RSSI(i);
        seen[s] = true;
      }
    }
  }
  if (found >= 0) WiFi.scanDelete();

  // Viditelne podle sily, za nimi neviditelne v poradi slotu.
  for (int pass = 0; pass < 2; pass++) {
    for (int s = 0; s < WIFI_SLOTS; s++) {
      if (!Settings_WifiSsid(s)[0] || seen[s] != (pass == 0)) continue;
      int k = n;
      while (pass == 0 && k > 0 && rssi[order[k - 1]] < rssi[s]) { order[k] = order[k - 1]; k--; }
      order[k] = s;
      n++;
    }
  }
  for (int i = 0; i < n; i++) {
    const int s = order[i];
    if (seen[s]) Serial.printf("WiFi: %s je v dosahu (%d dBm)\n", Settings_WifiSsid(s), rssi[s]);
    if (tryConnect(Settings_WifiSsid(s), Settings_WifiPass(s), perNetMs)) return true;
  }
  return false;
}

void WiFi_Begin() {
  gfx->fillScreen(C_BLACK);
  UI_TextCentered(T(S_WIFI_WAIT), LCD_HEIGHT / 2, C_WHITE, 2);
  gfx->flush();

  if (Settings_HasWifi()) {
    // Radio do vychoziho stavu - po restartu watchdogem v nem muze zustat
    // rozdelany pokus o spojeni.
    WiFi.persistent(false);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);

    for (int r = 0; r < WIFI_BOOT_ROUNDS; r++) {
      if (r > 0) {
        char msg[48];
        snprintf(msg, sizeof(msg), "%s (%d/%d)", T(S_WIFI_WAIT), r + 1, WIFI_BOOT_ROUNDS);
        gfx->fillScreen(C_BLACK);
        UI_TextCentered(msg, LCD_HEIGHT / 2, C_WHITE, 2);
        gfx->flush();
        Serial.printf("WiFi: kolo %d/%d\n", r + 1, WIFI_BOOT_ROUNDS);
        WiFi.disconnect(true);
        for (int i = 0; i < 30; i++) { Watchdog_Feed(); delay(100); }   // 3 s pauza
      }
      if (connectAny(r == 0 ? WIFI_TRY_MS : WIFI_CONNECT_TIMEOUT_MS)) return;
    }
    Serial.println("WiFi: zadna ulozena sit se nepripojila, spoustim portal");
  }

  startAP();
}

void WiFi_Loop() {
  // --- Rezim pristupoveho bodu -------------------------------------------
  if (s_ap) {
    // Nikdo se k portalu nepripojil a deska ma ulozenou sit -> restart a nove
    // pokusy. Typicky po restartu watchdogem, kdy router desku chvili odmital.
    if (WiFi.softAPgetStationNum() > 0) s_apIdleSince = millis();
    if ((WIFI_PORTAL_RESTART_MS) > 0 && Settings_HasWifi() &&
        millis() - s_apIdleSince > WIFI_PORTAL_RESTART_MS) {
      Serial.println("WiFi: portal bez pouziti, restartuji a zkusim ulozene site");
      delay(100);
      ESP.restart();
    }

    // Nekdo zadal sit na konfiguracni strance.
    if (!WebConfig_WantsWifiConnect()) return;
    WebConfig_ClearWifiConnect();

    gfx->fillScreen(C_BLACK);
    UI_TextCentered(T(S_WIFI_WAIT), LCD_HEIGHT / 2, C_WHITE, 2);
    gfx->flush();

    if (tryConnect(Settings_WifiSsid(0), Settings_WifiPass(0))) return;

    // Nepovedlo se - zpatky na pristupovy bod, at uzivatel muze zkusit znovu.
    // Zadana sit (je ve slotu 0) se ZAHAZUJE, jinak by se deska po restartu
    // tvrdohlave pokousela o sit, ktera nefunguje. Ostatni ulozene zustanou -
    // a kdyz je nektera z nich v dosahu, pripoji se misto ni.
    Settings_SetWifiSlot(0, "", nullptr);
    if (Settings_HasWifi() && connectAny(WIFI_TRY_MS)) return;
    startAP();
    WiFi_DrawApScreen();
    return;
  }

  // --- Bezny provoz -------------------------------------------------------
  if (WiFi.status() == WL_CONNECTED) { s_downSince = 0; return; }

  // Spadlo spojeni. Zkousime se vracet donekonecna a NEPADAME zpatky na
  // pristupovy bod: nejcastejsi pricina je restartovany router, ktery se za
  // chvili vrati. Shodit kvuli tomu desku do rezimu portalu by znamenalo, ze
  // se uzivatel musi jit rucne prihlasit po kazdem vypadku proudu.
  unsigned long now = millis();
  if (!s_downSince) s_downSince = now ? now : 1;
  if (now - s_lastRetry < WIFI_RETRY_MS) return;
  s_lastRetry = now;

  // Prvni minutu se zkousi jen sit, na ktere deska byla. Teprve potom se
  // projdou ostatni ulozene (0.8.0) - kancelar, chata, hotspot v telefonu.
  if (now - s_downSince < WIFI_SWITCH_AFTER_MS) {
    Serial.println("WiFi: spojeni ztraceno, zkousim znovu");
    WiFi.reconnect();
    return;
  }
  Serial.println("WiFi: porad bez spojeni, zkousim vsechny ulozene site");
  if (connectAny(WIFI_TRY_MS)) { s_downSince = 0; return; }
  s_downSince = millis();       // dalsi kolo zase nejdriv pres reconnect()
}

bool   WiFi_IsConnected() { return !s_ap && WiFi.status() == WL_CONNECTED; }
bool   WiFi_IsAP()        { return s_ap; }
String WiFi_SSID()        { return WiFi_IsConnected() ? WiFi.SSID() : String(T(S_NOT_CONNECTED)); }
String WiFi_IP()          { return WiFi_IsConnected() ? WiFi.localIP().toString() : String(PORTAL_IP); }

void WiFi_Reset() {
  Settings_ClearWifi();
  startAP();
  WiFi_DrawApScreen();
}

// -----------------------------------------------------------------------------
//  Obrazovka pristupoveho bodu (480x320): QR vlevo, postup vpravo.
// -----------------------------------------------------------------------------
void WiFi_DrawApScreen() {
  gfx->fillScreen(C_BLACK);

  UI_TextCentered(LT("Nastaveni WiFi", "Nastavení WiFi", "Nastavenie WiFi", "Nastavenie WiFi"), 14, C_CYAN, 2);
  {
    char v[56];
    snprintf(v, sizeof(v), "laskakit.cz  |  %s %s", LT("verze", "verze", "verzia", "verzia"), FW_VERSION);
    UI_TextCentered(v, 40, C_GRAY, 1);
  }

  const int qrSize = 196;
  const int qrX = 22, qrY = 60;
  UI_DrawWifiQR(AP_SSID, AP_PASSWORD, /*open=*/true, qrX, qrY, qrSize);

  const int tx = qrX + qrSize + 24;
  gfx->setTextSize(1); gfx->setTextColor(C_WHITE);
  gfx->setCursor(tx, 74);  gfx->print(LT("1) Naskenujte QR kod, nebo se", "1) Naskenujte QR kód, nebo se", "1) Naskenujte QR kod alebo sa", "1) Naskenujte QR kód alebo sa"));
  gfx->setCursor(tx, 86);  gfx->print(LT("   pripojte k siti:", "   připojte k síti:", "   pripojte k sieti:", "   pripojte k sieti:"));
  gfx->setTextColor(C_YELLOW);
  gfx->setCursor(tx, 104); gfx->print(AP_SSID);
  gfx->setTextColor(C_GRAY);
  gfx->setCursor(tx, 118); gfx->print(LT("(sit bez hesla)", "(síť bez hesla)", "(siet bez hesla)", "(sieť bez hesla)"));

  gfx->setTextColor(C_WHITE);
  gfx->setCursor(tx, 142); gfx->print(LT("2) V prohlizeci otevrete:", "2) V prohlížeči otevřete:", "2) V prehliadaci otvorte:", "2) V prehliadači otvorte:"));
  gfx->setTextColor(C_YELLOW);
  gfx->setTextSize(2);
  gfx->setCursor(tx, 158); gfx->print(PORTAL_IP);

  gfx->setTextSize(1); gfx->setTextColor(C_WHITE);
  gfx->setCursor(tx, 190); gfx->print(LT("3) Vyberte domaci WiFi", "3) Vyberte domácí WiFi", "3) Vyberte domacu WiFi", "3) Vyberte domácu WiFi"));
  gfx->setCursor(tx, 202); gfx->print(LT("   a zadejte heslo.", "   a zadejte heslo.", "   a zadajte heslo.", "   a zadajte heslo."));

  gfx->setTextColor(C_GRAY);
  gfx->setCursor(tx, 226); gfx->print(LT("Telefon nahlasi, ze sit nema", "Telefon nahlásí, že síť nemá", "Telefon ohlasi, ze siet nema", "Telefón ohlási, že sieť nemá"));
  gfx->setCursor(tx, 238); gfx->print(LT("internet - to nevadi.", "internet - to nevadí.", "internet - to nevadi.", "internet - to nevadí."));

  if ((WIFI_PORTAL_RESTART_MS) > 0 && Settings_HasWifi()) {
    char t[80];
    snprintf(t, sizeof(t), LT("Bez pripojeni se za %lu min restartuje a zkusi ulozene site.",
                              "Bez připojení se za %lu min restartuje a zkusí uložené sítě.",
                              "Bez pripojenia sa o %lu min restartuje a skusi ulozene siete.",
                              "Bez pripojenia sa o %lu min reštartuje a skúsi uložené siete."),
             (unsigned long)(WIFI_PORTAL_RESTART_MS / 60000UL));
    UI_TextCentered(t, 292, C_GRAY, 1);
  } else {
    UI_TextCentered(LT("Deska ceka, dokud sit nezadate. Zadny casovy limit.",
                       "Deska čeká, dokud síť nezadáte. Žádný časový limit.",
                       "Doska caka, kym nezadate siet. Ziadny casovy limit.",
                       "Doska čaká, kým nezadáte sieť. Žiadny časový limit."),
                    292, C_GRAY, 1);
  }
  gfx->flush();
}
