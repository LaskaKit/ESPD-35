/*
 * LVGL example for LaskaKit ESPD-3.5" 320x480, ILI9488
 *
 * How to steps:
 * 1. Copy file Setup300_ILI9488_ESPD-3_5.h from https://github.com/LaskaKit/ESPD-35/tree/main/SW to Arduino/libraries/TFT_eSPI/User_Setups/
 * 2. in Arduino/libraries/TFT_eSPI/User_Setup_Select.h
      a. comment: #include <User_Setup.h>
      b. add: #include <User_Setups/Setup300_ILI9488_ESPD-3_5.h>  // Setup file for LaskaKit ESPD-3.5" 320x480, ILI9488
   3. Add lv_conf.h into the Arduino Libraries directory (most often Documents/Arduino/libraries) next to the lvgl folder
 *
 * Email:podpora@laskakit.cz
 * Web:laskakit.cz
*/

#include <lvgl.h>
#include <TFT_eSPI.h>
#include "ui.h"
#include "FT6236.h"

// Set your version of display (V2.0 uses FT6234 touch driver and V2.1 uses FT5436)
//#define V2_0
#define V2_1

// TFT SPI
#define TFT_LED 33       // TFT backlight pin
#define TFT_LED_PWM 250  // dutyCycle 0-255 last minimum was 10

/*Change to your screen resolution*/
static const uint16_t screenWidth = 480;
static const uint16_t screenHeight = 320;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * screenHeight / 10];

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* TFT instance */
FT6236 ts = FT6236(screenWidth, screenHeight);

#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char *buf) {
  Serial.printf(buf);
  Serial.flush();
}
#endif

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp);
}

/*Read the touchpad*/
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  if (!ts.touched()) {
    data->state = LV_INDEV_STATE_REL;
  } else {
    uint16_t touchX, touchY;
    // Retrieve a point
    TS_Point p = ts.getPoint();
    touchX = p.x;
    touchY = p.y;
    Serial.println(touchX);
    Serial.println(touchY);

    data->state = LV_INDEV_STATE_PR;

    /*Set the coordinates*/
    data->point.x = touchX;
    data->point.y = touchY;
  }
}

void setup() {
  Serial.begin(115200); /* prepare for possible serial debug */

  String LVGL_Arduino = "Hello Arduino! ";
  LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

  Serial.println(LVGL_Arduino);
  Serial.println("I am LVGL_Arduino");

  lv_init();

#if LV_USE_LOG != 0
  lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif

  tft.begin();        /* TFT init */
  tft.setRotation(3); /* Landscape orientation, flipped */
	// configure backlight LED PWM functionalitites
  ledcAttach(TFT_LED, 1000, 8);
  ledcWrite(1, TFT_LED_PWM);

  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  /*Initialize the (dummy) input device driver*/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  if (!ts.begin(40))  // 40 in this case represents the sensitivity. Try higer or lower for better response.
  {
    Serial.println("Unable to start the capacitive touchscreen.");
  }
#ifdef V2_0
  ts.setRotation(3);
#endif
#ifdef V2_1
  ts.setRotation(1);
#endif

  ui_init();

  Serial.println("Setup done");
}

void loop() {
  lv_timer_handler(); /* let the GUI do its work */
  delay(5);
}
