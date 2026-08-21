// =============================================================================
//  ESPD35_MeteoPlaneRadar
//  Day / night brightness. See NightMode.h.
//
// =============================================================================
#include "NightMode.h"
#include "Settings.h"
#include "Forecast.h"
#include "Outside.h"
#include "UI.h"        // Backlight_Set + rozmery z Config.h
#include <time.h>

static bool s_applied = false;
static bool s_lastState = false;

void NightMode_Apply() {
  Backlight_Set(Settings_Backlight());
  s_applied = true;
}

void NightMode_Toggle() {
  if (Settings_NightAuto()) return;
  Settings_SetNight(!Settings_IsNight());
  NightMode_Apply();
}

void NightMode_Tick() {
  // Push the initial brightness once, even before anything else is known.
  if (!s_applied) NightMode_Apply();

  if (!Settings_NightAuto()) return;
  if (!Outside_TimeValid()) return;          // no clock yet - nothing to compare

  time_t rise = 0, set = 0;
  if (!Forecast_SunTimes(&rise, &set)) return;

  const long off = (long)Settings_NightOffsetMin() * 60L;
  const time_t nightEnds   = rise + off;     // dawn, shifted later by the offset
  const time_t nightStarts = set  - off;     // dusk, shifted earlier

  time_t now = time(nullptr);

  // Note on the day boundary: the sun times we hold are for the day of the last
  // fetch. Just after midnight they are yesterday's, so both events are already
  // in the past and the test below still says "night" - which is correct. The
  // next refresh (half hourly) replaces them with today's.
  bool night = (now < nightEnds) || (now >= nightStarts);

  if (night != s_lastState || !s_applied) {
    s_lastState = night;
    Settings_SetNight(night);
    NightMode_Apply();
    Serial.printf("Rezim: %s (jas %u%%)\n", night ? "nocni" : "denni",
                  (unsigned)Settings_Backlight());
  }
}
