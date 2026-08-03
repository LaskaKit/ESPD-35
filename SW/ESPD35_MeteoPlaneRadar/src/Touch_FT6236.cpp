// =============================================================================
//  ESPD35_MeteoPlaneRadar - kapacitni dotyk FT5436/FT6236 (I2C).
//  Prevzato z principu LaskaKit Touch_example.ino (chip FT5436, lib FT6236).
//
//  ODOLNOST VUCI VADNYM CTENIM (dulezite!)
//  --------------------------------------
//  Knihovna FT6236.cpp cte z I2C bez kontroly navratovych hodnot:
//
//      Wire.requestFrom((byte)FT6236_ADDR, (byte)16);
//      for (uint8_t i = 0; i < 16; i++) i2cdat[i] = Wire.read();
//
//  Kdyz prenos selze (rusenie na sbernici, kolize, radic zrovna nestiha),
//  Wire.read() vraci -1, do bufferu se ulozi 0xFF a data se dekoduji jako
//  "15 dotyku na souradnici (4095, 4095)". Knihovna sice pocet bodu > 2
//  srazi na nulu, ale getPoint() pak vrati TS_Point(0,0,0,...) - a po rotaci 3
//  je to bod (0, 320), tedy LEVY DOLNI ROH. Puvodni Touch_Read() navratovou
//  slozku z (priznak platnosti) ignoroval a hlasil to jako skutecne klepnuti.
//
//  Presne tohle zpusobuje "detail letadla se zavira sam od sebe": falesne
//  klepnuti mimo letadlo zrusi vyber. Stejny druh chyby resil i puvodni
//  projekt petus/MeteoPlaneRadar ve verzi 0.5.1 (tam radic CST820 vracel
//  samé 0xFF).
//
//  Nize se proto kazdy vzorek overuje:
//    1) z == 0            -> knihovna nemela platny bod (vc. pripadu samych 0xFF)
//    2) souradnice mimo displej (vc. zapornych po rotaci)
//    3) surova hodnota 0x0FFF = 4095 (typicky otisk prazdne sbernice)
//    4) nesmyslny skok behem jednoho doteku (TOUCH_MAX_JUMP_PX)
//
//  Knihovna FT6236.cpp se zamerne NEUPRAVUJE - LaskaKit ji distribuuje jako
//  prejatou a uzivatele muzou mit vlastni kopii. Vsechna ochrana je tady.
// =============================================================================
#include "Touch_FT6236.h"
#include "Config.h"
#include <Wire.h>
#include <stdlib.h>
#include "FT6236.h"     // knihovna prilozena LaskaKit (viz README)

// Rozmery odpovidaji displeji na sirku (rotace se resi setRotation nize).
static FT6236 ts = FT6236(LCD_WIDTH, LCD_HEIGHT);
static bool s_ok = false;

// Stav pro filtr skoku - posledni PRIJATY bod aktivniho doteku.
static bool s_active = false;
static int  s_lastX = 0, s_lastY = 0;

#if TOUCH_DEBUG
static uint32_t s_badReads    = 0;   // zahozene vzorky od posledniho vypisu
static uint32_t s_badReported = 0;   // millis() posledniho vypisu
#endif

// Zahozeny vzorek se hlasi nejvys jednou za sekundu, aby rusna sbernice
// nezahltila seriovou linku (a tim cely beh nezpomalila).
static void noteBadSample(const char* why) {
#if TOUCH_DEBUG
  s_badReads++;
  uint32_t now = millis();
  if (now - s_badReported >= 1000) {
    s_badReported = now;
    Serial.printf("TOUCH: zahozeno %lu vadnych cteni (%s)\n",
                  (unsigned long)s_badReads, why);
    s_badReads = 0;
  }
#else
  (void)why;
#endif
}

bool Touch_Init() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
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
  if (!s_ok) { s_active = false; return false; }

  // JEDNO cteni misto dvou. Puvodne se volalo ts.touched() (cteni registru
  // poctu bodu) a hned potom ts.getPoint() (cteni 16 bajtu) - dve nezavisle
  // I2C transakce kazdych 5 ms. Mezi nimi se stav sbernice mohl zmenit a
  // dvojnasobny provoz dvakrat zvysoval sanci na poskozene cteni.
  // getPoint() uvnitr vola readData(), takze pocet bodu si zjisti sam a
  // pri nulovem/nesmyslnem poctu vrati bod s z == 0.
  TS_Point p = ts.getPoint();

  // (1) Priznak platnosti. Pokryva i pripad, kdy prenos selhal a v bufferu
  //     jsou samé 0xFF (knihovna pak nastavi touches = 0).
  if (p.z == 0) {
    if (s_active) noteBadSample("neplatny bod (z=0)");
    s_active = false;
    return false;
  }

  // (2) Souradnice mimo displej. Po rotaci muze vyjit i zaporna hodnota,
  //     proto se testuje int, ne uint.
  if (p.x < 0 || p.y < 0 || p.x >= LCD_WIDTH || p.y >= LCD_HEIGHT) {
    noteBadSample("souradnice mimo displej");
    return false;   // s_active zamerne NErusime - jde o vypadek uprostred gesta
  }

  // (3) Otisk prazdne sbernice: surovych 0xFF dava 12bitovou hodnotu 4095.
  if (p.x == 4095 || p.y == 4095) {
    noteBadSample("hodnota 4095 (same 0xFF)");
    return false;
  }

  // (4) Filtr skoku. Behem jednoho doteku se prst za jednu smycku (~5 ms)
  //     nemuze presunout pres pul displeje. Vetsi skok je porucha cteni;
  //     zahodit ho je bezpecnejsi nez z nej udelat "swipe".
#if TOUCH_MAX_JUMP_PX > 0
  if (s_active) {
    int dx = p.x - s_lastX, dy = p.y - s_lastY;
    if (abs(dx) > TOUCH_MAX_JUMP_PX || abs(dy) > TOUCH_MAX_JUMP_PX) {
      noteBadSample("nesmyslny skok");
      return false;
    }
  }
#endif

  s_active = true;
  s_lastX = p.x; s_lastY = p.y;
  *x = p.x;
  *y = p.y;
  return true;
}
