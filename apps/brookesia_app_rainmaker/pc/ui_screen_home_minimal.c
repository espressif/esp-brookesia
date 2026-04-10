/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * PC-only lightweight Home screen: same symbols as ui_Screen_Home.c so Login can
 * lv_disp_load_scr(ui_Screen_Home) without pulling device manager / detail screens.
 */

#include "ui.h"
#include "ui_display.h"
#include "ui_i18n.h"
#include "screens/ui_Screen_Home.h"
#include "rainmaker_app_backend.h"
#include "lvgl.h"

#include <stddef.h>
#include <stdio.h>

lv_obj_t *ui_Screen_Home = NULL;
lv_obj_t *ui_Home_Label_HomeName = NULL;
lv_obj_t *ui_Home_Cont_DeviceGrid = NULL;

static lv_obj_t *s_title_lab;
static lv_obj_t *s_welcome_lab;
static lv_obj_t *s_subtitle_lab;
static lv_obj_t *s_nodes_hint;

void ui_Screen_Home_set_home_name(const char *home_name)
{
    if (ui_Home_Label_HomeName == NULL) {
        return;
    }
    if (home_name == NULL || home_name[0] == '\0') {
        home_name = ui_str(UI_STR_HOME_DEFAULT);
    }
    lv_obj_set_style_text_font(ui_Home_Label_HomeName, ui_font_welcome_card(), 0);
    lv_label_set_text(ui_Home_Label_HomeName, home_name);
}

void ui_Screen_Home_refresh_from_backend(void)
{
    if (!rm_app_backend_refresh_home_screen(true)) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_FAILED_REFRESH_HOME));
        return;
    }
    ui_Screen_Home_set_home_name(rm_app_backend_get_current_group_name());
    int n_devices = rm_app_backend_get_current_home_devices_count();
    if (s_nodes_hint) {
        char buf[120];
        snprintf(buf, sizeof(buf), "%d %s", n_devices, ui_str(UI_STR_DEVICE_PLURAL));
        lv_label_set_text(s_nodes_hint, buf);
    }
}

void ui_Screen_Home_update_device_card(rm_device_item_handle_t device)
{
    (void)device;
}

void ui_Screen_Home_reload_device_cards(void)
{
}

void ui_Screen_Home_apply_language(void)
{
    if (s_title_lab) {
        lv_label_set_text(s_title_lab, ui_str(UI_STR_MY_DEVICES));
        lv_obj_set_style_text_font(s_title_lab, ui_font_title(), 0);
    }
    if (s_welcome_lab) {
        lv_obj_set_style_text_font(s_welcome_lab, ui_font_welcome_card(), 0);
        lv_label_set_text(s_welcome_lab, ui_str(UI_STR_WELCOME_TO));
    }
    if (s_subtitle_lab) {
        lv_label_set_text(s_subtitle_lab, ui_str(UI_STR_COMMON));
        lv_obj_set_style_text_font(s_subtitle_lab, ui_font_body(), 0);
    }
    if (ui_Home_Label_HomeName) {
        const char *gn = rm_app_backend_get_current_group_name();
        lv_obj_set_style_text_font(ui_Home_Label_HomeName, ui_font_welcome_card(), 0);
        lv_label_set_text(ui_Home_Label_HomeName, (gn && gn[0]) ? gn : ui_str(UI_STR_HOME_DEFAULT));
    }
}

void ui_Screen_Home_screen_init(void)
{
    ui_Screen_Home = lv_obj_create(NULL);
    lv_obj_add_flag(ui_Screen_Home, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(ui_Screen_Home, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(ui_Screen_Home, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(ui_Screen_Home, lv_color_hex(0xF5F8FF), 0);

    const int32_t bar_h = ui_display_top_bar_height();
    lv_obj_t *bar = lv_obj_create(ui_Screen_Home);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(bar, lv_pct(100));
    lv_obj_set_height(bar, bar_h);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xF5F8FF), 0);
    lv_obj_set_style_border_width(bar, 0, 0);

    s_title_lab = lv_label_create(bar);
    lv_label_set_text(s_title_lab, ui_str(UI_STR_MY_DEVICES));
    lv_obj_set_style_text_font(s_title_lab, ui_font_title(), 0);
    lv_obj_align(s_title_lab, LV_ALIGN_TOP_MID, 0, ui_display_scale_px(8));

    const lv_font_t *fw = ui_font_welcome_card();
    const int32_t lh = lv_font_get_line_height(fw);
    const int32_t wpad = ui_display_scale_px(16);
    const int32_t wgap = ui_display_scale_px(4);
    const int32_t min_card_h = wpad * 2 + lh + wgap + LV_MAX(ui_display_scale_px(28), lh + ui_display_scale_px(6));

    lv_obj_t *welcome = lv_obj_create(ui_Screen_Home);
    lv_obj_clear_flag(welcome, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(welcome, lv_pct(92));
    lv_obj_set_height(welcome, LV_MAX(ui_display_scale_px(100), min_card_h));
    lv_obj_align(welcome, LV_ALIGN_TOP_MID, 0, bar_h + ui_display_scale_px(8));
    lv_obj_set_style_bg_color(welcome, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(welcome, ui_display_scale_px(14), 0);
    lv_obj_set_style_border_width(welcome, 0, 0);
    lv_obj_set_style_shadow_width(welcome, ui_display_scale_px(12), 0);
    lv_obj_set_style_shadow_color(welcome, lv_color_hex(0xB0C4EE), 0);
    lv_obj_set_style_pad_all(welcome, wpad, 0);

    s_welcome_lab = lv_label_create(welcome);
    lv_obj_remove_style_all(s_welcome_lab);
    lv_obj_set_style_text_font(s_welcome_lab, fw, 0);
    lv_label_set_text(s_welcome_lab, ui_str(UI_STR_WELCOME_TO));
    lv_label_set_long_mode(s_welcome_lab, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_welcome_lab, lv_pct(48));
    lv_obj_align(s_welcome_lab, LV_ALIGN_TOP_LEFT, 0, 0);

    ui_Home_Label_HomeName = lv_label_create(welcome);
    lv_obj_remove_style_all(ui_Home_Label_HomeName);
    lv_obj_set_style_text_font(ui_Home_Label_HomeName, fw, 0);
    lv_label_set_text(ui_Home_Label_HomeName, ui_str(UI_STR_HOME_DEFAULT));
    lv_obj_set_style_text_color(ui_Home_Label_HomeName, lv_color_hex(0x2E6BE6), 0);
    lv_obj_set_width(ui_Home_Label_HomeName, lv_pct(78));
    lv_label_set_long_mode(ui_Home_Label_HomeName, LV_LABEL_LONG_CLIP);
    lv_obj_align_to(ui_Home_Label_HomeName, s_welcome_lab, LV_ALIGN_OUT_BOTTOM_LEFT, 0, wgap);

    s_subtitle_lab = lv_label_create(ui_Screen_Home);
    lv_label_set_text(s_subtitle_lab, ui_str(UI_STR_COMMON));
    lv_obj_set_style_text_font(s_subtitle_lab, ui_font_body(), 0);
    lv_obj_align_to(s_subtitle_lab, welcome, LV_ALIGN_OUT_BOTTOM_LEFT, ui_display_scale_px(24), ui_display_scale_px(12));

    ui_Home_Cont_DeviceGrid = lv_obj_create(ui_Screen_Home);
    lv_obj_clear_flag(ui_Home_Cont_DeviceGrid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(ui_Home_Cont_DeviceGrid, lv_pct(92));
    lv_obj_set_height(ui_Home_Cont_DeviceGrid, ui_display_scale_px(200));
    lv_obj_align_to(ui_Home_Cont_DeviceGrid, s_subtitle_lab, LV_ALIGN_OUT_BOTTOM_MID, 0, ui_display_scale_px(28));
    lv_obj_set_style_bg_opa(ui_Home_Cont_DeviceGrid, LV_OPA_0, 0);
    lv_obj_set_style_border_width(ui_Home_Cont_DeviceGrid, 0, 0);
    lv_obj_set_flex_flow(ui_Home_Cont_DeviceGrid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(ui_Home_Cont_DeviceGrid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(ui_Home_Cont_DeviceGrid, ui_display_scale_px(10), 0);
    lv_obj_set_style_pad_row(ui_Home_Cont_DeviceGrid, ui_display_scale_px(10), 0);

    s_nodes_hint = lv_label_create(ui_Home_Cont_DeviceGrid);
    lv_label_set_text(s_nodes_hint, "");
    lv_obj_set_style_text_font(s_nodes_hint, ui_font_body(), 0);
    lv_obj_set_style_text_color(s_nodes_hint, lv_color_hex(0x64748B), 0);
}

void ui_Screen_Home_screen_destroy(void)
{
    if (ui_Screen_Home) {
        lv_obj_del(ui_Screen_Home);
    }
    ui_Screen_Home = NULL;
    ui_Home_Label_HomeName = NULL;
    ui_Home_Cont_DeviceGrid = NULL;
    s_title_lab = NULL;
    s_welcome_lab = NULL;
    s_subtitle_lab = NULL;
    s_nodes_hint = NULL;
}
