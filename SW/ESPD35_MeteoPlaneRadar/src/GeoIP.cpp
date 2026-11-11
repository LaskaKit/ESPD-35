// =============================================================================
//  ESPD35_MeteoPlaneRadar - automaticka detekce polohy podle verejne IP adresy.
//  Zdroj: ip-api.com. Rucne zadana poloha (ve WiFi portalu) ma prednost.
// =============================================================================
#include "GeoIP.h"
#include "Settings.h"
#include "Net.h"

#include <WiFi.h>
#include <ArduinoJson.h>

#define GEOIP_URL "http://ip-api.com/json/?fields=status,lat,lon,city"

bool GeoIP_DetectIfNeeded() {
  // Uzivatelem zadana poloha ma prednost - nesahame na ni.
  if (Settings_HasLocation()) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  // Pres sdilene Net_GetString(): ip-api.com je obycejne http://, ale telo se
  // i tak cte pres sink, ktery zvladne chunked a ma strop. Vlastni getString()
  // tady nemel ani jedno.
  String payload;
  if (!Net_GetString(GEOIP_URL, payload, "GeoIP")) return false;

  JsonDocument doc;
  if (deserializeJson(doc, payload)) { Serial.println("GeoIP: chyba JSON"); return false; }
  if (String(doc["status"] | "") != "success") { Serial.println("GeoIP: neuspech"); return false; }

  double lat = doc["lat"] | 0.0;
  double lon = doc["lon"] | 0.0;
  // Sanity check (Evropa-ish), aby ojedinela chyba nehodila radar mimo.
  if (lat < 35.0 || lat > 72.0 || lon < -25.0 || lon > 45.0) {
    Serial.printf("GeoIP: %.4f, %.4f mimo ocekavany rozsah - ignoruji\n", lat, lon);
    return false;
  }

  const char* city = doc["city"] | "";
  Serial.printf("GeoIP: %.4f, %.4f%s%s\n", lat, lon, city[0] ? " - " : "", city);
  Settings_SetLocation(lat, lon);
  return true;
}
