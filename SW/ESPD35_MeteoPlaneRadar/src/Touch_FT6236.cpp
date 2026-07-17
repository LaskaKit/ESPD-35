// =============================================================================
//  ESPD35_MeteoPlaneRadar - kapacitni dotyk FT5436/FT6236 (I2C).
//  Prevzato z principu LaskaKit Touch_example.ino (chip FT5436, lib FT6236).
// =============================================================================
#include "Touch_FT6236.h"
#include "Config.h"
#include <Wire.h>
#include "FT6236.h"     // knihovna prilozena LaskaKit (viz README)

// Rozmery odpovidaji displeji na sirku (rotace se resi setRotation nize).
static FT6236 ts = FT6236(LCD_WIDTH, LCD_HEIGHT);
static bool s_ok = false;

bool Touch_Init() {
  Wire.begin(I2C_SDA, I2C_SCL);
#if defined(TOUCH_INT) && (TOUCH_INT >= 0)
  pinMode(TOUCH_INT, INPUT_PULLUP);
#endif
  s_ok = ts.begin(TOUCH_SENSITIVITY);
  if (!s_ok) {
    Serial.println("Touch: FT5436/FT6236 nenalezen - zkontrolujte I2C_SDA/SCL v Config.h");
  } else {
    ts.setRotation(TOUCH_ROTATION);   // 3 = v2.1+ (FT5436), 1 = v2 a starsi
    Serial.println("Touch: OK");
  }
  return s_ok;
}

bool Touch_Read(int* x, int* y) {
  if (!s_ok) return false;
  if (!ts.touched()) return false;
  TS_Point p = ts.getPoint();
  *x = p.x;
  *y = p.y;
  return true;
}
