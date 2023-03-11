/*
 * LVGL example for LaskaKit ESPD-3.5" 320x480, ILI9488
 *
 * How to steps:
 * 1. Copy file Setup300_ILI9488_ESPD-3_5.h from https://github.com/LaskaKit/ESPD-35/tree/main/SW to Arduino/libraries/TFT_eSPI/User_Setups/
 * 2. in Arduino/libraries/TFT_eSPI/User_Setup_Select.h
      a. comment: #include <User_Setup.h>
      b. add: #include <User_Setups/Setup300_ILI9488_ESPD-3_5.h>  // Setup file for LaskaKit ESPD-3.5" 320x480, ILI9488
 *
 * Email:podpora@laskakit.cz
 * Web:laskakit.cz
*/

#include <lvgl.h>
#include <TFT_eSPI.h>
#include "FT6236.h"

/*Change to your screen resolution*/
static const uint16_t screenWidth = 480;
static const uint16_t screenHeight = 320;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 10];

// TFT SPI
#define TFT_LED 33      // TFT backlight pin
#define TFT_LED_PWM 10 // dutyCycle 0-255 last minimum was 15

FT6236 ts = FT6236(screenWidth, screenHeight);
TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* TFT instance */

static void slider_event_cb(lv_event_t *e);
static lv_obj_t *slider_label;

static void slider_event_cb(lv_event_t *e)
{
  lv_obj_t *slider = lv_event_get_target(e);
  char buf[8];
  lv_snprintf(buf, sizeof(buf), "%d%%", (int)lv_slider_get_value(slider));
  lv_label_set_text(slider_label, buf);
  lv_obj_align_to(slider_label, slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
}

static void slider_event_led(lv_event_t *e)
{
  uint16_t light;
  lv_obj_t *slider = lv_event_get_target(e);
  light = map((int)lv_slider_get_value(slider), 0, 100, 10, 255);
  ledcWrite(1, light); // dutyCycle of backlight
}

void lv_example_slider(void)
{
  /*Create a slider in the center of the display*/
  lv_obj_t *slider = lv_slider_create(lv_scr_act());
  lv_obj_center(slider);
  lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

  /*Set backlight brightness*/
  lv_obj_add_event_cb(slider, slider_event_led, LV_EVENT_VALUE_CHANGED, NULL);

  /*Create a label below the slider*/
  slider_label = lv_label_create(lv_scr_act());
  lv_label_set_text(slider_label, "0%");

  lv_obj_align_to(slider_label, slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
}

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp);
}

/*Read the touchpad*/
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
  if (!ts.touched())
  {
    data->state = LV_INDEV_STATE_REL;
  }
  else
  {
    uint16_t touchX, touchY;
    // Retrieve a point
    TS_Point p = ts.getPoint();
    touchX = p.x;
    touchY = p.y;

    data->state = LV_INDEV_STATE_PR;

    /*Set the coordinates*/
    data->point.x = touchX;
    data->point.y = touchY;
  }
}

void setup()
{
  Serial.begin(115200); /* prepare for possible serial debug */

  lv_init();
  ledcSetup(1, 5000, 8);     // ledChannel, freq, resolution
  ledcAttachPin(TFT_LED, 1); // ledPin, ledChannel
  ledcWrite(1, TFT_LED_PWM); // dutyCycle 0-255
  tft.begin();               /* TFT init */
  tft.setRotation(3);        /* Landscape orientation, flipped */

  if (!ts.begin(40)) // 40 in this case represents the sensitivity. Try higer or lower for better response.
  {
    Serial.println("Unable to start the capacitive touchscreen.");
  }
  ts.setRotation(3);

  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10);

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

  /*Set slider*/
  lv_example_slider();
}

void loop()
{
  lv_timer_handler(); /* let the GUI do its work */
  delay(5);
}
