// =============================================================================
//  ESPD35_MeteoPlaneRadar - WiFi. Viz WiFiPortal.h.
// =============================================================================
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
  Serial.printf("WiFi: pristupovy bod %s, adresa %s\n", AP_SSID, PORTAL_IP);
  WebConfig_Begin(true);
}

static bool tryConnect(const char* ssid, const char* pass) {
  Serial.printf("WiFi: pripojuji k %s\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);          // spici radio pridava desitky ms ke kazdemu GET
  WiFi.begin(ssid, pass);
  if (!waitForConnect(WIFI_CONNECT_TIMEOUT_MS)) {
    Serial.println("WiFi: pripojeni se nezdarilo");
    return false;
  }
  s_ap = false;
  Serial.printf("WiFi ok, IP %s\n", WiFi.localIP().toString().c_str());
  WebConfig_Begin(false);
  return true;
}

void WiFi_Begin() {
  gfx->fillScreen(C_BLACK);
  UI_TextCentered(T(S_WIFI_WAIT), LCD_HEIGHT / 2, C_WHITE, 2);
  gfx->flush();

  if (Settings_HasWifi() &&
      tryConnect(Settings_WifiSsid(), Settings_WifiPass())) return;

  startAP();
}

void WiFi_Loop() {
  // --- Rezim pristupoveho bodu -------------------------------------------
  if (s_ap) {
    // Nekdo zadal sit na konfiguracni strance.
    if (!WebConfig_WantsWifiConnect()) return;
    WebConfig_ClearWifiConnect();

    gfx->fillScreen(C_BLACK);
    UI_TextCentered(T(S_WIFI_WAIT), LCD_HEIGHT / 2, C_WHITE, 2);
    gfx->flush();

    if (tryConnect(Settings_WifiSsid(), Settings_WifiPass())) return;

    // Nepovedlo se - zpatky na pristupovy bod, at uzivatel muze zkusit znovu.
    // Udaje se ZAHAZUJI, jinak by se deska po restartu tvrdohlave pokousela
    // o sit, ktera nefunguje, misto aby nabidla portal.
    Settings_ClearWifi();
    startAP();
    WiFi_DrawApScreen();
    return;
  }

  // --- Bezny provoz -------------------------------------------------------
  if (WiFi.status() == WL_CONNECTED) return;

  // Spadlo spojeni. Zkousime se vracet donekonecna a NEPADAME zpatky na
  // pristupovy bod: nejcastejsi pricina je restartovany router, ktery se za
  // chvili vrati. Shodit kvuli tomu desku do rezimu portalu by znamenalo, ze
  // se uzivatel musi jit rucne prihlasit po kazdem vypadku proudu.
  unsigned long now = millis();
  if (now - s_lastRetry < WIFI_RETRY_MS) return;
  s_lastRetry = now;
  Serial.println("WiFi: spojeni ztraceno, zkousim znovu");
  WiFi.reconnect();
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

  UI_TextCentered("Nastaveni WiFi", 14, C_CYAN, 2);
  {
    char v[56];
    snprintf(v, sizeof(v), "laskakit.cz  |  verze %s", FW_VERSION);
    UI_TextCentered(v, 40, C_GRAY, 1);
  }

  const int qrSize = 196;
  const int qrX = 22, qrY = 60;
  UI_DrawWifiQR(AP_SSID, AP_PASSWORD, /*open=*/true, qrX, qrY, qrSize);

  const int tx = qrX + qrSize + 24;
  gfx->setTextSize(1); gfx->setTextColor(C_WHITE);
  gfx->setCursor(tx, 74);  gfx->print("1) Naskenujte QR kod, nebo se");
  gfx->setCursor(tx, 86);  gfx->print("   pripojte k siti:");
  gfx->setTextColor(C_YELLOW);
  gfx->setCursor(tx, 104); gfx->print(AP_SSID);
  gfx->setTextColor(C_GRAY);
  gfx->setCursor(tx, 118); gfx->print("(sit bez hesla)");

  gfx->setTextColor(C_WHITE);
  gfx->setCursor(tx, 142); gfx->print("2) V prohlizeci otevrete:");
  gfx->setTextColor(C_YELLOW);
  gfx->setTextSize(2);
  gfx->setCursor(tx, 158); gfx->print(PORTAL_IP);

  gfx->setTextSize(1); gfx->setTextColor(C_WHITE);
  gfx->setCursor(tx, 190); gfx->print("3) Vyberte domaci WiFi");
  gfx->setCursor(tx, 202); gfx->print("   a zadejte heslo.");

  gfx->setTextColor(C_GRAY);
  gfx->setCursor(tx, 226); gfx->print("Telefon nahlasi, ze sit nema");
  gfx->setCursor(tx, 238); gfx->print("internet - to nevadi.");

  UI_TextCentered("Deska ceka, dokud sit nezadate. Zadny casovy limit.",
                  292, C_GRAY, 1);
  gfx->flush();
}
