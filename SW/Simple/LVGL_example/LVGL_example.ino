/* 
 * LVGL example for LaskaKit ESPD-3.5" 320x480, ILI9488 https://www.laskakit.cz/laskakit-espd-35-esp32-3-5-tft-ili9488-touch/
 * example from TFT_eSPI library is used
 * 
 * How to steps:
 * 1. Copy file from https://github.com/LaskaKit/ESPD-35/tree/main/SW to Arduino/libraries/TFT_eSPI/User_Setups/
 *    - for version v2.3 and before:  Setup300_ILI9488_ESPD-3_5_v2.h 
 *    - for version v3 and above:     Setup303_ILI9488_ESPD-3_5_v3.h
 * 2. in Arduino/libraries/TFT_eSPI/User_Setup_Select.h 
      a. comment: #include <User_Setup.h> 
      b. add: 
          - for version v2.3 and before:  #include <User_Setups/Setup300_ILI9488_ESPD-3_5_v2.h>  // Setup file for LaskaKit ESPD-3.5" 320x480, ILI9488 
          - for version v3 and above:     #include <User_Setups/Setup303_ILI9488_ESPD-3_5_v3.h>  // Setup file for LaskaKit ESPD-3.5" 320x480, ILI9488 V3
 * 
 * Board constants:
      TFT_BL          - LED back-light use: analogWrite(TFT_BL, TFT_BL_PWM);
      POWER_OFF_PIN   - Pull LOW to switch board off
      TOUCH_INT       - Touch interrupt pin
    * I2C (µŠup and devices (only from v3)):
      I2C_SDA         - Data pin 
      I2C_SCL         - Clock pin
    * SPI (µŠup (only from v3) and SD card):
      SPI_MISO        - MISO pin
      SPI_MOSI        - MOSI pin
      SPI_SCK         - Clock pin
      SPI_USUP_CS     - µŠup Chip Select pin (only from v3)
      SPI_SD_CS       - SD Card Chip Select pin
    * I2S (only from v3):
      I2S_LRC         - Word select a.k.a. left-right clock pin
      I2S_DOUT        - Serial data pin
      I2S_BCLK        - Serial clock a.k.a. bit clock pin
    * Battery mesurement:
      BAT_PIN         - Battery voltage mesurement
      deviderRatio    - Voltage devider ratio on ADC pin 1MOhm + 1.3MOhm
 *
 * Touch: 
 * Chip used in board is FT5436, library: https://github.com/DustinWatts/FT6236
 * Just changed CHIPID and VENDID
 * Library is included in the project so it does not need to be downloaded
 *
 * Email:podpora@laskakit.cz
 * Web:laskakit.cz
*/

#include <lvgl.h>       // working on v8.4.0. Not working on v9 and newer
#include <TFT_eSPI.h>
#include "FT6236.h"

#define UPDATE_INTERVAL 1000   // 1 s
#define MEAS_POINTS  30
#define TFT_BL_PWM 255 // Backlight brightness 0-255

/*Change to your screen resolution*/
static const uint16_t screenWidth = 480;
static const uint16_t screenHeight = 320;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 10];

FT6236 ts = FT6236(screenWidth, screenHeight);
TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* TFT instance */

// Variables for LVGL
static lv_obj_t *slider_label;
lv_obj_t *chart;
lv_chart_series_t *ser;
uint16_t values[MEAS_POINTS + 1];

// Event for slider text
static void slider_event_cb(lv_event_t *e) 
{
  lv_obj_t *slider = lv_event_get_target(e);
  char buf[17];
  lv_snprintf(buf, sizeof(buf), "Brightness: %d%%", (int)lv_slider_get_value(slider));
  lv_label_set_text(slider_label, buf);
  lv_obj_align_to(slider_label, slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
}

// Event for changing brightness
static void slider_event_led(lv_event_t *e) 
{
  uint16_t light;
  lv_obj_t *slider = lv_event_get_target(e);
  light = map((int)lv_slider_get_value(slider), 0, 100, 10, 255);
  analogWrite(TFT_BL, light);      // Set brightness of backlight

}

// Slider object
void lv_example_slider(void) 
{
  /*Create a slider in the center of the display*/
  lv_obj_t *slider = lv_slider_create(lv_scr_act());
  lv_obj_align(slider, LV_ALIGN_CENTER, 0, 100);
  lv_slider_set_value(slider, 100, LV_ANIM_ON);
  lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

  /*Set backlight brightness*/
  lv_obj_add_event_cb(slider, slider_event_led, LV_EVENT_VALUE_CHANGED, NULL);

  // Set slider color
  static lv_style_t style_slider;
  lv_style_init(&style_slider);
  lv_style_set_bg_color(&style_slider, lv_color_hex(0x009baa));
  lv_obj_add_style(slider, &style_slider, LV_PART_INDICATOR);
  lv_obj_add_style(slider, &style_slider, LV_PART_MAIN);
  lv_obj_add_style(slider, &style_slider, LV_PART_KNOB);

  /*Create a label below the slider*/
  slider_label = lv_label_create(lv_scr_act());
  lv_label_set_text(slider_label, "Brightness: 100%");

  lv_obj_align_to(slider_label, slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
}

// Chart object
void lv_example_chart(void) 
{
  /*Create a chart*/
  chart = lv_chart_create(lv_scr_act());
  lv_obj_set_size(chart, 380, 150);
  lv_obj_align(chart, LV_ALIGN_CENTER, 0, -10);
  lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);
  lv_chart_set_point_count(chart, MEAS_POINTS);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 2800, 4200);
  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 5, 2, 8, 2, true, 50);
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE); /*Show lines and points too*/

  // Change data color
  ser = lv_chart_add_series(chart, lv_color_hex(0x009baa), LV_CHART_AXIS_PRIMARY_Y);

  /*Create a label below the slider*/
  static lv_obj_t *chart_label = lv_label_create(lv_scr_act());
  lv_label_set_text(chart_label, "Battery voltage in mV");
  lv_obj_align_to(chart_label, chart, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
}

// ADC read and chart update
void ADCUpdate() 
{
  static unsigned long previousMillis;
  unsigned long currentMillis = millis();
  if ((currentMillis - previousMillis) >= UPDATE_INTERVAL) 
  {
    // save the last time you blinked the LED
    previousMillis = currentMillis;

    for (uint16_t k = 0; k < MEAS_POINTS; k++)
    {        
      values[k] = values[k + 1];
      lv_chart_set_next_value(chart, ser, values[k]);
    }
    values[MEAS_POINTS] = analogReadMilliVolts(BAT_PIN) * deviderRatio;
    lv_chart_set_next_value(chart, ser, values[MEAS_POINTS]);
  }
}

// Button touch handler
static void btn_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
      pinMode(POWER_OFF_PIN, OUTPUT);
			digitalWrite(POWER_OFF_PIN, LOW);
    }
}

// Button object
void lv_example_btn(void)
{
  lv_obj_t * label;
  lv_obj_t * btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(btn, 100, 60);
  lv_obj_add_event_cb(btn, btn_event_handler, LV_EVENT_ALL, NULL);
  lv_obj_align(btn, LV_ALIGN_CENTER, 190, -130);

  // Change button color
  static lv_style_t style_btn;
  lv_style_init(&style_btn);
  lv_style_set_bg_color(&style_btn, lv_color_hex(0x009baa));
  lv_obj_add_style(btn, &style_btn, 0);

  // Add button label
  label = lv_label_create(btn);
  lv_label_set_text(label, "Power off");
  lv_obj_center(label);
}

// Image object
void lv_example_img(void)
{
  LV_IMG_DECLARE(laskakit);
  lv_obj_t * img = lv_img_create(lv_scr_act());
  lv_img_set_src(img, &laskakit);
  lv_obj_align(img, LV_ALIGN_CENTER, -190, -130);
  lv_obj_set_size(img, 60, 60);
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
  tft.begin();                /* TFT init */
  tft.setRotation(3);         /* Landscape orientation, flipped */

  analogWrite(TFT_BL, TFT_BL_PWM);      // Set brightness of backlight

	Wire.begin(I2C_SDA, I2C_SCL);            // set dedicated I2C pins for ESPD-3.5 board

	if (!ts.begin(40))	{ 		  // 40 in this case represents the sensitivity. Try higer or lower for better response.
		Serial.println("Unable to start the capacitive touchscreen.");
	}
  //ts.setRotation(1);		//for older version v2 and before, uses FT6234 touch driver
  ts.setRotation(1);		// FT5436 touch driver for v2.1 and above

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

  /*Initialize touch driver*/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  /*Set slider*/
  lv_example_slider();
  /*Set chart*/
  lv_example_chart();
  /*Set button*/
  lv_example_btn();
  /*Set image*/
  lv_example_img();
}

void loop() 
{
  ADCUpdate();
  lv_timer_handler(); /* let the GUI do its work */
}