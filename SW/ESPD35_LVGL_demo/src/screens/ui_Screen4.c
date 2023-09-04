// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.3.1
// LVGL version: 8.3.6
// Project name: ESPD35

#include "../ui.h"

void ui_Screen4_screen_init(void)
{
    ui_Screen4 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen4, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Image4 = lv_img_create(ui_Screen4);
    lv_img_set_src(ui_Image4, &ui_img_wp1862222_png);
    lv_obj_set_width(ui_Image4, LV_SIZE_CONTENT);   /// 480
    lv_obj_set_height(ui_Image4, LV_SIZE_CONTENT);    /// 320
    lv_obj_set_align(ui_Image4, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Image4, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_Image4, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Label5 = lv_label_create(ui_Screen4);
    lv_obj_set_width(ui_Label5, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label5, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Label5, 0);
    lv_obj_set_y(ui_Label5, 30);
    lv_obj_set_align(ui_Label5, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_Label5, "Keyboard");
    lv_obj_set_style_text_font(ui_Label5, &lv_font_montserrat_26, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Keyboard1 = lv_keyboard_create(ui_Screen4);
    lv_obj_set_width(ui_Keyboard1, 480);
    lv_obj_set_height(ui_Keyboard1, 170);
    lv_obj_set_align(ui_Keyboard1, LV_ALIGN_BOTTOM_MID);

    ui_TextArea1 = lv_textarea_create(ui_Screen4);
    lv_obj_set_width(ui_TextArea1, 480);
    lv_obj_set_height(ui_TextArea1, 70);
    lv_obj_set_x(ui_TextArea1, 0);
    lv_obj_set_y(ui_TextArea1, -47);
    lv_obj_set_align(ui_TextArea1, LV_ALIGN_CENTER);
    lv_textarea_set_placeholder_text(ui_TextArea1, "Placeholder...");

    lv_keyboard_set_textarea(ui_Keyboard1, ui_TextArea1);
    lv_obj_add_event_cb(ui_Screen4, ui_event_Screen4, LV_EVENT_ALL, NULL);

}