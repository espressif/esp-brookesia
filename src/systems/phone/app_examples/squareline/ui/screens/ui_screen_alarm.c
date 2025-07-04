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

void ui_screen_alarm_screen_init(void)
{
    ui_screen_alarm = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_screen_alarm, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_screen_alarm, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_screen_alarm, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_src(ui_screen_alarm, &ui_img_pattern_png, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_tiled(ui_screen_alarm, true, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_alarm_panel_alarm_container = lv_obj_create(ui_screen_alarm);
    lv_obj_set_width(ui_alarm_panel_alarm_container, lv_pct(100));
    lv_obj_set_height(ui_alarm_panel_alarm_container, lv_pct(100));
    lv_obj_set_align(ui_alarm_panel_alarm_container, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_color(ui_alarm_panel_alarm_container, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alarm_panel_alarm_container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_alarm_small_label_set_alarm = ui_label_small_label_create(ui_alarm_panel_alarm_container);
    lv_obj_set_x(ui_alarm_small_label_set_alarm, 0);
    lv_obj_set_y(ui_alarm_small_label_set_alarm, 17);
    lv_obj_set_align(ui_alarm_small_label_set_alarm, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_alarm_small_label_set_alarm, "Set alarm");
    lv_obj_set_style_text_color(ui_alarm_small_label_set_alarm, lv_color_hex(0x000746), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_alarm_small_label_set_alarm, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_alarm_alarm_comp_alarm_comp = ui_panel_alarm_comp_create(ui_alarm_panel_alarm_container);
    lv_obj_set_x(ui_alarm_alarm_comp_alarm_comp, 0);
    lv_obj_set_y(ui_alarm_alarm_comp_alarm_comp, 43);





    ui_alarm_alarm_comp_alarm_comp1 = ui_panel_alarm_comp_create(ui_alarm_panel_alarm_container);
    lv_obj_set_x(ui_alarm_alarm_comp_alarm_comp1, 0);
    lv_obj_set_y(ui_alarm_alarm_comp_alarm_comp1, 128);

    lv_label_set_text(ui_comp_get_child(ui_alarm_alarm_comp_alarm_comp1, UI_COMP_PANEL_ALARM_COMP_LABEL_ALARM_NUM2),
                      "8:00");

    lv_label_set_text(ui_comp_get_child(ui_alarm_alarm_comp_alarm_comp1, UI_COMP_PANEL_ALARM_COMP_SMALL_LABEL_PERIOD),
                      "Breakfast");

    lv_obj_add_state(ui_comp_get_child(ui_alarm_alarm_comp_alarm_comp1, UI_COMP_PANEL_ALARM_COMP_SWITCH_SWITCH1),
                     LV_STATE_CHECKED);      /// States


    ui_alarm_alarm_comp_alarm_comp2 = ui_panel_alarm_comp_create(ui_alarm_panel_alarm_container);
    lv_obj_set_x(ui_alarm_alarm_comp_alarm_comp2, 0);
    lv_obj_set_y(ui_alarm_alarm_comp_alarm_comp2, 213);

    lv_label_set_text(ui_comp_get_child(ui_alarm_alarm_comp_alarm_comp2, UI_COMP_PANEL_ALARM_COMP_LABEL_ALARM_NUM2),
                      "9:30");

    lv_label_set_text(ui_comp_get_child(ui_alarm_alarm_comp_alarm_comp2, UI_COMP_PANEL_ALARM_COMP_SMALL_LABEL_PERIOD),
                      "Yoga");



    ui_alarm_alarm_comp_alarm_comp3 = ui_panel_alarm_comp_create(ui_alarm_panel_alarm_container);
    lv_obj_set_x(ui_alarm_alarm_comp_alarm_comp3, 0);
    lv_obj_set_y(ui_alarm_alarm_comp_alarm_comp3, 298);
    lv_obj_set_style_border_color(ui_alarm_alarm_comp_alarm_comp3, lv_color_hex(0x293062), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_alarm_alarm_comp_alarm_comp3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_alarm_alarm_comp_alarm_comp3, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_alarm_alarm_comp_alarm_comp3, LV_BORDER_SIDE_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_label_set_text(ui_comp_get_child(ui_alarm_alarm_comp_alarm_comp3, UI_COMP_PANEL_ALARM_COMP_LABEL_ALARM_NUM2),
                      "11:00");

    lv_label_set_text(ui_comp_get_child(ui_alarm_alarm_comp_alarm_comp3, UI_COMP_PANEL_ALARM_COMP_SMALL_LABEL_PERIOD),
                      "Sleep");



    ui_alarm_scrolldots_scrolldots5 = ui_panel_scrolldots_create(ui_screen_alarm);
    lv_obj_set_x(ui_alarm_scrolldots_scrolldots5, 0);
    lv_obj_set_y(ui_alarm_scrolldots_scrolldots5, -8);

    lv_obj_set_width(ui_comp_get_child(ui_alarm_scrolldots_scrolldots5, UI_COMP_PANEL_SCROLLDOTS_PANEL_D1), 4);
    lv_obj_set_height(ui_comp_get_child(ui_alarm_scrolldots_scrolldots5, UI_COMP_PANEL_SCROLLDOTS_PANEL_D1), 4);

    lv_obj_set_x(ui_comp_get_child(ui_alarm_scrolldots_scrolldots5, UI_COMP_PANEL_SCROLLDOTS_PANEL_D2), 10);
    lv_obj_set_y(ui_comp_get_child(ui_alarm_scrolldots_scrolldots5, UI_COMP_PANEL_SCROLLDOTS_PANEL_D2), 0);

    lv_obj_set_x(ui_comp_get_child(ui_alarm_scrolldots_scrolldots5, UI_COMP_PANEL_SCROLLDOTS_PANEL_D3), 20);
    lv_obj_set_y(ui_comp_get_child(ui_alarm_scrolldots_scrolldots5, UI_COMP_PANEL_SCROLLDOTS_PANEL_D3), 0);

    lv_obj_set_x(ui_comp_get_child(ui_alarm_scrolldots_scrolldots5, UI_COMP_PANEL_SCROLLDOTS_PANEL_D4), 30);
    lv_obj_set_y(ui_comp_get_child(ui_alarm_scrolldots_scrolldots5, UI_COMP_PANEL_SCROLLDOTS_PANEL_D4), 0);

    lv_obj_set_x(ui_comp_get_child(ui_alarm_scrolldots_scrolldots5, UI_COMP_PANEL_SCROLLDOTS_PANEL_D5), 40);
    lv_obj_set_y(ui_comp_get_child(ui_alarm_scrolldots_scrolldots5, UI_COMP_PANEL_SCROLLDOTS_PANEL_D5), 0);

    lv_obj_set_width(ui_comp_get_child(ui_alarm_scrolldots_scrolldots5, UI_COMP_PANEL_SCROLLDOTS_PANEL_D6), 8);
    lv_obj_set_height(ui_comp_get_child(ui_alarm_scrolldots_scrolldots5, UI_COMP_PANEL_SCROLLDOTS_PANEL_D6), 8);
    lv_obj_set_x(ui_comp_get_child(ui_alarm_scrolldots_scrolldots5, UI_COMP_PANEL_SCROLLDOTS_PANEL_D6), 50);
    lv_obj_set_y(ui_comp_get_child(ui_alarm_scrolldots_scrolldots5, UI_COMP_PANEL_SCROLLDOTS_PANEL_D6), 0);
    lv_obj_set_style_bg_color(ui_comp_get_child(ui_alarm_scrolldots_scrolldots5, UI_COMP_PANEL_SCROLLDOTS_PANEL_D6),
                              lv_color_hex(0x101C52), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_comp_get_child(ui_alarm_scrolldots_scrolldots5, UI_COMP_PANEL_SCROLLDOTS_PANEL_D6), 255,
                            LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_screen_alarm, ui_event_screen_alarm, LV_EVENT_ALL, NULL);

}
