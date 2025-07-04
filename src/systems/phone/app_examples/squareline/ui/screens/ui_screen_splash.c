/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.5.0
// LVGL version: 9.1.0
// Project name: Smart_Gadget

#include "../ui.h"

void ui_screen_splash_screen_init(void)
{
    ui_screen_splash = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_screen_splash, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_screen_splash, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_screen_splash, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_splash_small_label_demo = ui_label_small_label_create(ui_screen_splash);
    lv_obj_set_x(ui_splash_small_label_demo, 0);
    lv_obj_set_y(ui_splash_small_label_demo, 75);
    lv_obj_set_align(ui_splash_small_label_demo, LV_ALIGN_CENTER);
    lv_label_set_text(ui_splash_small_label_demo, "Demo");
    lv_obj_set_style_text_color(ui_splash_small_label_demo, lv_color_hex(0x9C9CD9), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_splash_small_label_demo, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_splash_small_label_smart_gadget = ui_label_small_label_create(ui_screen_splash);
    lv_obj_set_x(ui_splash_small_label_smart_gadget, 0);
    lv_obj_set_y(ui_splash_small_label_smart_gadget, 50);
    lv_obj_set_align(ui_splash_small_label_smart_gadget, LV_ALIGN_CENTER);
    lv_label_set_text(ui_splash_small_label_smart_gadget, "Smart Gadget");
    lv_obj_set_style_text_color(ui_splash_small_label_smart_gadget, lv_color_hex(0x000746),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_splash_small_label_smart_gadget, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_splash_image_sls_logo = lv_image_create(ui_screen_splash);
    lv_image_set_src(ui_splash_image_sls_logo, &ui_img_sls_logo_png);
    lv_obj_set_width(ui_splash_image_sls_logo, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_splash_image_sls_logo, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_splash_image_sls_logo, 0);
    lv_obj_set_y(ui_splash_image_sls_logo, -50);
    lv_obj_set_align(ui_splash_image_sls_logo, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_splash_image_sls_logo, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_remove_flag(ui_splash_image_sls_logo, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    lv_obj_add_event_cb(ui_screen_splash, ui_event_screen_splash, LV_EVENT_ALL, NULL);

}
