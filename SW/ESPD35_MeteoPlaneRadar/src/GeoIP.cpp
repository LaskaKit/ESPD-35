// =============================================================================
//  ESPD35_MeteoPlaneRadar - automaticka detekce polohy podle verejne IP adresy.
//  Zdroj: ip-api.com. Rucne zadana poloha (ve WiFi portalu) ma prednost.
// =============================================================================
#include "GeoIP.h"
#include "Settings.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define GEOIP_URL "http://ip-api.com/json/?fields=status,lat,lon,city"

bool GeoIP_DetectIfNeeded() {
  // Uzivatelem zadana poloha ma prednost - nesahame na ni.
  if (Settings_HasLocation()) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  http.setTimeout(6000);
  if (!http.begin(GEOIP_URL)) return false;
  int code = http.GET();
  if (code != HTTP_CODE_OK) { http.end(); return false; }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) return false;
  if (String(doc["status"] | "") != "success") return false;

  double lat = doc["lat"] | 0.0;
  double lon = doc["lon"] | 0.0;
  // Sanity check (Evropa-ish), aby ojedinela chyba nehodila radar mimo.
  if (lat < 35.0 || lat > 72.0 || lon < -25.0 || lon > 45.0) return false;

  Settings_SetLocation(lat, lon);
  return true;
}
