// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.3.1
// LVGL version: 8.3.6
// Project name: ESPD35

#ifndef _ESPD35_UI_H
#define _ESPD35_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined __has_include
#if __has_include("lvgl.h")
#include "lvgl.h"
#elif __has_include("lvgl/lvgl.h")
#include "lvgl/lvgl.h"
#else
#include "lvgl.h"
#endif
#else
#include "lvgl.h"
#endif

#include "ui_helpers.h"
#include "ui_events.h"
// SCREEN: ui_Screen1
void ui_Screen1_screen_init(void);
void ui_event_Screen1(lv_event_t * e);
extern lv_obj_t * ui_Screen1;
extern lv_obj_t * ui_Image1;
extern lv_obj_t * ui_Label8;
extern lv_obj_t * ui_Spinner1;
// SCREEN: ui_Screen2
void ui_Screen2_screen_init(void);
void ui_event_Screen2(lv_event_t * e);
extern lv_obj_t * ui_Screen2;
extern lv_obj_t * ui_Image2;
extern lv_obj_t * ui_Chart1;
extern lv_obj_t * ui_Label1;
// SCREEN: ui_Screen3
void ui_Screen3_screen_init(void);
void ui_event_Screen3(lv_event_t * e);
extern lv_obj_t * ui_Screen3;
extern lv_obj_t * ui_Image3;
extern lv_obj_t * ui_Label2;
extern lv_obj_t * ui_Bar1;
void ui_event_Button1(lv_event_t * e);
extern lv_obj_t * ui_Button1;
extern lv_obj_t * ui_Label3;
void ui_event_Button2(lv_event_t * e);
extern lv_obj_t * ui_Button2;
void ui_event_Label4(lv_event_t * e);
extern lv_obj_t * ui_Label4;
// SCREEN: ui_Screen4
void ui_Screen4_screen_init(void);
void ui_event_Screen4(lv_event_t * e);
extern lv_obj_t * ui_Screen4;
extern lv_obj_t * ui_Image4;
extern lv_obj_t * ui_Label5;
extern lv_obj_t * ui_Keyboard1;
extern lv_obj_t * ui_TextArea1;
// SCREEN: ui_Screen5
void ui_Screen5_screen_init(void);
void ui_event_Screen5(lv_event_t * e);
extern lv_obj_t * ui_Screen5;
extern lv_obj_t * ui_Image5;
extern lv_obj_t * ui_Label6;
extern lv_obj_t * ui_Dropdown1;
extern lv_obj_t * ui_Roller1;
extern lv_obj_t * ui_Switch1;
extern lv_obj_t * ui_Switch2;
extern lv_obj_t * ui_Switch3;
// SCREEN: ui_Screen6
void ui_Screen6_screen_init(void);
void ui_event_Screen6(lv_event_t * e);
extern lv_obj_t * ui_Screen6;
extern lv_obj_t * ui_Image6;
extern lv_obj_t * ui_Label7;
extern lv_obj_t * ui_Calendar1;
extern lv_obj_t * ui____initial_actions0;

LV_IMG_DECLARE(ui_img_822791001);    // assets/Návrh bez názvu (3).png
LV_IMG_DECLARE(ui_img_wp1862222_png);    // assets/wp1862222.png

void ui_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
