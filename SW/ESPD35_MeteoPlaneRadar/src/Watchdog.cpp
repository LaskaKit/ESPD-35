// =============================================================================
//  ESPD35_MeteoPlaneRadar - hardwarovy watchdog (Task WDT).
//  Kompatibilni s ESP32 Arduino core 2.x i 3.x.
// =============================================================================
#include "Watchdog.h"
#include "esp_task_wdt.h"
#include "esp_arduino_version.h"

#define WDT_TIMEOUT_S 30

void Watchdog_Begin() {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  // Core 3.x: WDT uz bezi, jen ho prekonfigurujeme a prihlasime tento task.
  esp_task_wdt_config_t cfg = {
    .timeout_ms = WDT_TIMEOUT_S * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true,
  };
  esp_task_wdt_reconfigure(&cfg);
  esp_task_wdt_add(NULL);
#else
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);
#endif
}

void Watchdog_Feed()    { esp_task_wdt_reset(); }
void Watchdog_Suspend() { esp_task_wdt_delete(NULL); }
void Watchdog_Resume()  { esp_task_wdt_add(NULL); }
