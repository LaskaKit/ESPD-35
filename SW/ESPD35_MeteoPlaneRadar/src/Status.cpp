// =============================================================================
//  ESPD35_MeteoPlaneRadar - stavove poznamky zdroju dat. Viz Status.h.
// =============================================================================
#include "Status.h"
#include <stdarg.h>
#include <stdio.h>

#define STATUS_TEXT_MAX 48

static char          s_txt[ST_COUNT][STATUS_TEXT_MAX];
static unsigned long s_at[ST_COUNT] = {0};

static const char* SLOT_NAME[ST_COUNT] = { "Letadla", "Meteoradar", "Predpoved" };

void Status_Set(StatusSlot slot, const char* fmt, ...) {
  if (slot >= ST_COUNT) return;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(s_txt[slot], STATUS_TEXT_MAX, fmt, ap);
  va_end(ap);
  // millis() == 0 by se poznalo jako "nikdy nenastaveno", proto minimalne 1.
  unsigned long now = millis();
  s_at[slot] = now ? now : 1;
}

void Status_Text(StatusSlot slot, char* out, size_t cap) {
  if (!out || cap == 0) return;
  out[0] = '\0';
  if (slot >= ST_COUNT) return;
  if (!s_at[slot]) { snprintf(out, cap, "-"); return; }

  unsigned long age = (millis() - s_at[slot]) / 1000UL;
  if (age < 90)        snprintf(out, cap, "%s (pred %lu s)",   s_txt[slot], age);
  else if (age < 5400) snprintf(out, cap, "%s (pred %lu min)", s_txt[slot], age / 60);
  else                 snprintf(out, cap, "%s (pred %lu h)",   s_txt[slot], age / 3600);
}

void Status_Dump() {
  char buf[STATUS_TEXT_MAX + 24];
  for (uint8_t i = 0; i < ST_COUNT; i++) {
    Status_Text((StatusSlot)i, buf, sizeof(buf));
    Serial.printf("  %-11s %s\n", SLOT_NAME[i], buf);
  }
}
