// =============================================================================
//  ESPD35_MeteoPlaneRadar - aktualizace firmwaru pres WiFi (ElegantOTA).
//
//  ElegantOTA se pouziva ve VYCHOZIM (synchronnim) rezimu nad WebServer, ktery
//  je soucasti ESP32 core. Nic se v knihovne needituje a ESPAsyncWebServer ani
//  AsyncTCP nejsou potreba - odpada tim i znama kolize HTTP_* enumu mezi
//  ESPAsyncWebServer.h a http_parser.h z core.
//
//  PROGRESS BAR
//  ------------
//  Puvodni projekt petus/MeteoPlaneRadar prubeh zamerne NEkresli a misto toho
//  na dobu zapisu zhasina podsviceni: tam je RGB panel ST7701, ktery si obraz
//  prubezne cte z PSRAM pres DMA, a zapis do flash mu (kvuli suspendovani cache)
//  data odrezava - obraz by poskakoval.
//
//  ESPD-3.5 ma ILI9488 na SPI. Ten si obraz drzi ve vlastni pameti a data mu
//  posilame my, ve stejnem tasku, ktery zapisuje flash - nikdy tedy nebezi
//  soucasne. Prubeh proto kreslit MUZEME, coz je proti zdrojovemu projektu
//  vylepseni: uzivatel vidi postup i bez prohlizece.
//
//  Prekresluje se jen po celych 2 %, protoze jeden flush je prenos celeho
//  snimku (480*320*2 = 300 kB) po SPI, tedy radove desitky ms. Pri kresleni
//  kazdeho paketu by se nahravani vyrazne zpomalilo.
// =============================================================================
#include "OTA.h"
#include "UI.h"
#include "Config.h"
#include "Version.h"
#include "Watchdog.h"
#include "Settings.h"
#include "Touch_FT6236.h"

#include <WiFi.h>
#include <WebServer.h>      // soucast ESP32 core - zadna knihovna navic
#include <ElegantOTA.h>

static WebServer s_server(80);
static volatile bool s_busy = false;    // probiha nahravani
static int  s_lastPct = -1;

// --- Uvodni obrazovka: QR kod na AP + postup ---
static void drawInfo() {
  gfx->fillScreen(C_BLACK);

  UI_TextCentered("Aktualizace firmware", 14, C_CYAN, 2);
  {
    char v[48];
    snprintf(v, sizeof(v), "laskakit.cz  |  nyni verze %s", FW_VERSION);
    UI_TextCentered(v, 40, C_GRAY, 1);
  }

  // QR vlevo, postup vpravo (stejne rozvrzeni jako AP obrazovka portalu).
  const int qrSize = 190;
  const int qrX = 24, qrY = 62;
  UI_DrawWifiQR(AP_SSID, "", /*open=*/true, qrX, qrY, qrSize);

  const int tx = qrX + qrSize + 24;
  gfx->setTextSize(1); gfx->setTextColor(C_WHITE);
  gfx->setCursor(tx, 76);  gfx->print("1) Pripojte se na WiFi:");
  gfx->setTextColor(C_YELLOW);
  gfx->setCursor(tx, 92);  gfx->print(AP_SSID);
  gfx->setTextColor(C_GRAY);
  gfx->setCursor(tx, 106); gfx->print("(sit bez hesla)");

  gfx->setTextColor(C_WHITE);
  gfx->setCursor(tx, 130); gfx->print("2) V prohlizeci otevrete:");
  gfx->setTextColor(C_YELLOW);
  gfx->setCursor(tx, 146); gfx->print("192.168.4.1/update");

  gfx->setTextColor(C_WHITE);
  gfx->setCursor(tx, 170); gfx->print("3) Nahrajte soubor");
  gfx->setTextColor(C_CYAN);
  gfx->setCursor(tx, 186); gfx->print("ESPD35_MeteoPlaneRadar");
  gfx->setCursor(tx, 198); gfx->print(".ino.bin  (bez 'merged')");

  gfx->setTextColor(C_GRAY);
  gfx->setCursor(tx, 226); gfx->print("Klepnutim na displej");
  gfx->setCursor(tx, 238); gfx->print("se vratite zpet (restart).");

  UI_TextCentered("Telefon nahlasi, ze sit nema internet - to nevadi.", 280, C_GRAY, 1);
  UI_TextCentered("Pri neuspechu zustane puvodni verze.", 296, C_GRAY, 1);
  gfx->flush();
}

// --- Obrazovka prubehu ---
static void drawProgress(int pct) {
  gfx->fillScreen(C_BLACK);
  UI_TextCentered("Probiha aktualizace", 90, C_WHITE, 2);
  UI_TextCentered("Neodpojujte napajeni", 118, C_YELLOW, 1);

  const int bx = 60, by = 150, bw = LCD_WIDTH - 120, bh = 34;
  gfx->drawRoundRect(bx, by, bw, bh, 6, C_GRAY);
  int fill = (bw - 4) * pct / 100;
  if (fill > 0) gfx->fillRoundRect(bx + 2, by + 2, fill, bh - 4, 4, C_GREEN);

  char t[8];
  snprintf(t, sizeof(t), "%d%%", pct);
  UI_TextCentered(t, by + bh + 16, C_WHITE, 2);
  UI_TextCentered("Po dokonceni se deska sama restartuje.", 250, C_GRAY, 1);
  gfx->flush();
}

// --- Callbacky ElegantOTA ---
static void onStart() {
  s_busy = true;
  s_lastPct = -1;
  drawProgress(0);
}

static void onProgress(size_t cur, size_t total) {
  Watchdog_Feed();
  if (!total) return;
  int pct = (int)((uint64_t)cur * 100 / total);
  if (pct > 100) pct = 100;
  // Prekreslujeme jen po 2 % - viz komentar v hlavicce souboru.
  if (pct / 2 == s_lastPct / 2) return;
  s_lastPct = pct;
  drawProgress(pct);
}

static void onEnd(bool ok) {
  gfx->fillScreen(C_BLACK);
  UI_TextCentered(ok ? "Hotovo, restartuji..." : "Aktualizace selhala",
                  LCD_HEIGHT / 2 - 10, ok ? C_GREEN : C_RED, 2);
  if (!ok) UI_TextCentered("Puvodni verze zustava v desce.",
                           LCD_HEIGHT / 2 + 20, C_GRAY, 1);
  gfx->flush();
  // Pri uspechu restartuje desku sama ElegantOTA (setAutoReboot).
  if (!ok) { delay(2500); s_busy = false; }
}

void OTA_Run() {
  Serial.println("OTA: spoustim rezim aktualizace");

  // Otevreny AP s pevnou, znamou adresou (192.168.4.1).
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);
  drawInfo();

  ElegantOTA.begin(&s_server);
  ElegantOTA.setAutoReboot(true);
  ElegantOTA.onStart(onStart);
  ElegantOTA.onProgress(onProgress);
  ElegantOTA.onEnd(onEnd);
  s_server.begin();

  // Cekame, dokud nas aktualizace nerestartuje, nebo dokud uzivatel neklepne
  // na displej / neuplyne OTA_IDLE_MS.
  bool armed = false;                 // klepnuti se prijme az po zvednuti prstu
  unsigned long start = millis();
  for (;;) {
    s_server.handleClient();
    ElegantOTA.loop();
    Watchdog_Feed();

    if (!s_busy) {
      int tx, ty;
      if (!Touch_Read(&tx, &ty)) armed = true;
      else if (armed)            break;    // klepnuti (po uvolneni) = odchod
      if (millis() - start > OTA_IDLE_MS) {
        Serial.println("OTA: vyprsel cas necinnosti");
        break;
      }
    }
    delay(5);
  }

  // Cisty odchod: zrusit AP a restartovat. Po restartu se deska pripoji
  // k domaci WiFi a obnovi posledni obrazovku i rozsah (ulozene v NVS).
  s_server.stop();
  WiFi.softAPdisconnect(true);
  delay(100);
  ESP.restart();
}
