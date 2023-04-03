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
#include <ESP32AnalogRead.h>
#include <TFT_eSPI.h>
#include "FT6236.h"

// Set your version of display (V2.0 uses FT6234 touch driver and V2.1 uses FT5436)
#define V2_0
//#define V2_1

/* ADC */
#define UPDATE_INTERVAL 1000   // 1 s
#define MEAS_POINTS  30
#define ADC 34  // Battery voltage mesurement
#define deviderRatio 1.3
ESP32AnalogRead adc;

#define POWER_OFF_PIN 17

/*Change to your screen resolution*/
static const uint16_t screenWidth = 480;
static const uint16_t screenHeight = 320;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 10];

// TFT SPI
#define TFT_LED 33       // TFT backlight pin
#define TFT_LED_PWM 250  // dutyCycle 0-255 last minimum was 10

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
  ledcWrite(1, light);  // dutyCycle of backlight
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
    values[MEAS_POINTS] = adc.readVoltage() * deviderRatio * 1000;
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
  adc.attach(ADC);
  lv_init();
  ledcSetup(1, 5000, 8);      // ledChannel, freq, resolution
  ledcAttachPin(TFT_LED, 1);  // ledPin, ledChannel
  ledcWrite(1, TFT_LED_PWM);  // dutyCycle 0-255
  tft.begin();                /* TFT init */
  tft.setRotation(3);         /* Landscape orientation, flipped */

  if (!ts.begin(40))  // 40 in this case represents the sensitivity. Try higer or lower for better response.
  {
    Serial.println("Unable to start the capacitive touchscreen.");
  }
  #ifdef V2_0
    ts.setRotation(1);
  #endif
  #ifdef V2_1
    ts.setRotation(3);
  #endif

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