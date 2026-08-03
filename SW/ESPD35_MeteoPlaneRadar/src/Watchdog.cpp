// =============================================================================
//  ESPD35_MeteoPlaneRadar - hardwarovy watchdog (Task WDT) pro provoz 24/7.
//  Kompatibilni s ESP32 Arduino core 2.x i 3.x.
//
//  Proti puvodni verzi pribyl priznak s_subscribed. Bez nej se stavalo, ze:
//    - kdyz esp_task_wdt_add() v Begin() selhal, Feed() dal volal
//      esp_task_wdt_reset() na neprihlasenem tasku (tichy no-op, ale vypadalo
//      to, ze watchdog bezi),
//    - Resume() mohl task prihlasit podruhe.
//  Timeout se ridi konstantou WDT_TIMEOUT_S v Config.h.
// =============================================================================
#include "Watchdog.h"
#include "esp_task_wdt.h"
#include "esp_arduino_version.h"

static bool s_subscribed = false;

void Watchdog_Begin() {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  // Core 3.x: WDT uz bezi (inicializuje ho Arduino), jen ho prekonfigurujeme
  // a prihlasime tento task. Kdyby jeste nebezel, inicializujeme ho sami.
  esp_task_wdt_config_t cfg = {
    .timeout_ms = WDT_TIMEOUT_S * 1000,
    .idle_core_mask = 0,       // hlidame jen nasi smycku, ne idle tasky
    .trigger_panic = true,     // pri vyprseni restart
  };
  if (esp_task_wdt_reconfigure(&cfg) != ESP_OK) {
    esp_task_wdt_init(&cfg);
  }
#else
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
#endif
  s_subscribed = (esp_task_wdt_add(NULL) == ESP_OK);
  if (!s_subscribed) Serial.println("Watchdog: prihlaseni tasku selhalo");
}

void Watchdog_Feed() {
  if (s_subscribed) esp_task_wdt_reset();
}

// Pred blokujici operaci (WiFi portal), ktera si watchdog krmit neumi.
void Watchdog_Suspend() {
  if (s_subscribed) { esp_task_wdt_delete(NULL); s_subscribed = false; }
}

void Watchdog_Resume() {
  if (!s_subscribed && esp_task_wdt_add(NULL) == ESP_OK) {
    s_subscribed = true;
    esp_task_wdt_reset();
  }
}
