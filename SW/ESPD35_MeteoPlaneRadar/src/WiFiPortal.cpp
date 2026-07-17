// =============================================================================
//  ESPD35_MeteoPlaneRadar - WiFi pripojeni + konfiguracni AP portal s QR kodem.
//  Port z petus/Meteo-PlaneRadar, AP obrazovka prizpusobena 480x320.
// =============================================================================
#include "WiFiPortal.h"
#include "Settings.h"
#include "UI.h"
#include "Config.h"

#include <WiFi.h>
#include <WiFiManager.h>

static unsigned long s_lastReconnect = 0;

// Vykresli na displej navod pro pripojeni k AP (obdelnik 480x320).
static void drawApScreen() {
  gfx->fillScreen(C_BLACK);

  UI_TextCentered("ESPD35_MeteoPlaneRadar", 24, C_CYAN, 2);
  UI_TextCentered("laskakit.cz", 50, C_GRAY, 1);
  UI_TextCentered("Naskenujte telefonem:", 66, C_GRAY, 1);

  // QR vlevo, textova napoveda vpravo.
  int qrSize = 210;
  int qrX = 20;
  int qrY = 88;
  UI_DrawWifiQR(AP_SSID, AP_PASSWORD, /*open=*/true, qrX, qrY, qrSize);

  int tx = qrX + qrSize + 24;
  gfx->setTextSize(2); gfx->setTextColor(C_WHITE);
  gfx->setCursor(tx, 110); gfx->print("1) Pripojte");
  gfx->setCursor(tx, 134); gfx->print("   WiFi sit:");
  gfx->setTextColor(C_YELLOW);
  gfx->setCursor(tx, 162); gfx->print("ESPD35-MeteoPlane");
  gfx->setCursor(tx, 184); gfx->print("Radar-Setup");
  gfx->setTextColor(C_GRAY); gfx->setTextSize(1);
  gfx->setCursor(tx, 214); gfx->print("(sit bez hesla)");
  gfx->setTextColor(C_WHITE); gfx->setTextSize(2);
  gfx->setCursor(tx, 236); gfx->print("2) Otevrete");
  gfx->setCursor(tx, 258); gfx->print("   192.168.4.1");

  UI_TextCentered("Zpet: Exit v portalu, nebo pockejte 3 min", 306, C_GRAY, 1);
  gfx->flush();   // canvas -> panel
}

static const char* apPass() {
  return (strlen(AP_PASSWORD) == 0) ? nullptr : AP_PASSWORD;
}

static void onAP(WiFiManager* wm) { drawApScreen(); }

static void saveParams(WiFiManagerParameter& pLat, WiFiManagerParameter& pLon) {
  double lat = atof(pLat.getValue());
  double lon = atof(pLon.getValue());
  if (lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180 && (lat != 0 || lon != 0)) {
    Settings_SetLocation(lat, lon);
  }
}

bool WiFi_ConnectOrPortal() {
  gfx->fillScreen(C_BLACK);
  UI_TextCentered("Pripojuji WiFi...", LCD_HEIGHT / 2, C_WHITE, 2);
  gfx->flush();

  char latBuf[24], lonBuf[24];
  snprintf(latBuf, sizeof(latBuf), "%.5f", Settings_Lat());
  snprintf(lonBuf, sizeof(lonBuf), "%.5f", Settings_Lon());

  WiFiManager wm;
  wm.setDebugOutput(false);
  wm.setConfigPortalTimeout(180);
  wm.setConnectTimeout(20);
  wm.setAPCallback(onAP);

  WiFiManagerParameter pLat("lat", "Zemepisna sirka (lat)", latBuf, 24);
  WiFiManagerParameter pLon("lon", "Zemepisna delka (lon)", lonBuf, 24);
  wm.addParameter(&pLat);
  wm.addParameter(&pLon);
  wm.setSaveParamsCallback([&] { saveParams(pLat, pLon); });

  bool ok = wm.autoConnect(AP_SSID, apPass());
  if (ok) {
    saveParams(pLat, pLon);
  }
  return ok;
}

void WiFi_StartPortal() {
  char latBuf[24], lonBuf[24];
  snprintf(latBuf, sizeof(latBuf), "%.5f", Settings_Lat());
  snprintf(lonBuf, sizeof(lonBuf), "%.5f", Settings_Lon());

  WiFiManager wm;
  wm.setDebugOutput(false);
  wm.setConfigPortalTimeout(180);
  wm.setAPCallback(onAP);
  const char* menu[] = {"wifi", "info", "exit"};
  wm.setMenu(menu, 3);
  WiFiManagerParameter pLat("lat", "Zemepisna sirka (lat)", latBuf, 24);
  WiFiManagerParameter pLon("lon", "Zemepisna delka (lon)", lonBuf, 24);
  wm.addParameter(&pLat);
  wm.addParameter(&pLon);
  wm.setSaveParamsCallback([&] { saveParams(pLat, pLon); });
  wm.startConfigPortal(AP_SSID, apPass());
  saveParams(pLat, pLon);
}

void WiFi_Loop() {
  if (WiFi.status() == WL_CONNECTED) return;
  unsigned long now = millis();
  if (now - s_lastReconnect < 15000) return;
  s_lastReconnect = now;
  WiFi.reconnect();
}

bool   WiFi_IsConnected() { return WiFi.status() == WL_CONNECTED; }
String WiFi_SSID() { return WiFi_IsConnected() ? WiFi.SSID() : String("(nepripojeno)"); }
String WiFi_IP()   { return WiFi_IsConnected() ? WiFi.localIP().toString() : String("-"); }
void   WiFi_Reset() { WiFiManager wm; wm.resetSettings(); }
