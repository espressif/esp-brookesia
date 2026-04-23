/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// Home screen (hand-written based on UI design)

#include "../ui.h"
#include "../ui_gesture_nav.h"
#include "rainmaker_app_backend.h"
#include "rainmaker_device_group_manager.h"
#include "ui_Screen_Light_Detail.h"
#include "ui_Screen_Switch_Details.h"
#include "ui_Screen_Fan_Details.h"
#include "rainmaker_standard_device.h"
#ifndef RM_HOST_BUILD
#include "esp_system.h"
#include "esp_memory_utils.h"
#endif

lv_obj_t *ui_Screen_Home = NULL;
lv_obj_t *ui_Home_Label_HomeName = NULL;
lv_obj_t *ui_Home_Cont_DeviceGrid = NULL;

/* Static labels for i18n refresh */
static lv_obj_t *s_home_title_lab = NULL;
static lv_obj_t *s_home_welcome_lab = NULL;
static lv_obj_t *s_home_common_lab = NULL;

/* Refresh button (Common row) */
static lv_obj_t *s_home_common_refresh_btn = NULL;
static lv_obj_t *s_home_common_refresh_icon = NULL;
static lv_obj_t *s_home_common_refresh_spinner = NULL;
static bool s_home_refreshing = false;

/* Welcome "Home >" dropdown */
static lv_obj_t *s_home_dropdown_panel = NULL;   /* list container, child of screen */
static lv_obj_t *s_home_group_selector_btn = NULL; /* clickable area over "Home >" */
typedef struct ui_home_card_ctx_t {
    rm_device_item_handle_t handle;
    lv_obj_t *card;
    lv_obj_t *icon;
    lv_obj_t *name;
    lv_obj_t *sw;
    struct ui_home_card_ctx_t *next;
} ui_home_card_ctx_t;

static ui_home_card_ctx_t *s_home_card_ctx_head = NULL;
static void ui_Screen_Home_set_devices(int device_count);

/** Device card size for current layout (set when grid is created). */
static int32_t s_home_card_w = 132;
static int32_t s_home_card_h = 104;

/**
 * P4 (1024x600) and similar wide screens: use more columns; card width fills grid.
 * Reference design: 132x104 @ ~480px-wide layout.
 */
static void ui_home_compute_card_dims(void)
{
    int32_t hor = ui_display_get_hor_res();
    if (hor <= 0) {
        hor = 480;
    }
    int32_t grid_w = (hor * 92) / 100;
    int32_t pad = ui_display_scale_px(10);
    int32_t cols = 2;
    if (hor >= 960) {
        cols = 4;
    } else if (hor >= 640) {
        cols = 3;
    }
    int32_t w = (grid_w - pad * (cols - 1)) / cols;
    int32_t min_w = ui_display_scale_px(120);
    int32_t max_w = ui_display_scale_px(340);
    if (w < min_w) {
        w = min_w;
    }
    if (w > max_w) {
        w = max_w;
    }
    s_home_card_w = w;
    s_home_card_h = (w * 104) / 132;
    if (s_home_card_h < ui_display_scale_px(88)) {
        s_home_card_h = ui_display_scale_px(104);
    }
}

typedef struct {
    rm_device_item_handle_t device;
    lv_obj_t *icon;
} ui_home_power_ctx_t;

static void ui_home_common_refresh_set_loading(bool loading)
{
    if (!s_home_common_refresh_btn) {
        return;
    }
    s_home_refreshing = loading;
    if (loading) {
        lv_obj_add_state(s_home_common_refresh_btn, LV_STATE_DISABLED);
        lv_obj_set_style_opa(s_home_common_refresh_btn, LV_OPA_70, 0);
        if (s_home_common_refresh_icon) {
            lv_obj_add_flag(s_home_common_refresh_icon, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_home_common_refresh_spinner) {
            lv_obj_clear_flag(s_home_common_refresh_spinner, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(s_home_common_refresh_spinner);
        }
    } else {
        lv_obj_clear_state(s_home_common_refresh_btn, LV_STATE_DISABLED);
        lv_obj_set_style_opa(s_home_common_refresh_btn, LV_OPA_100, 0);
        if (s_home_common_refresh_icon) {
            lv_obj_clear_flag(s_home_common_refresh_icon, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_home_common_refresh_spinner) {
            lv_obj_add_flag(s_home_common_refresh_spinner, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void ui_home_common_refresh_timer_cb(lv_timer_t *t)
{
    if (t) {
        lv_timer_del(t);
    }
    ui_Screen_Home_refresh_from_backend();
    ui_home_common_refresh_set_loading(false);
}

static void ui_home_common_refresh_btn_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (s_home_refreshing) {
        return;
    }
    ui_home_common_refresh_set_loading(true);
    ui_show_busy(ui_str(UI_STR_HOME_REFRESHING));
    /* Defer refresh to let UI update first */
    (void)lv_timer_create(ui_home_common_refresh_timer_cb, 10, NULL);
}

static void ui_home_plus_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_show_msgbox(ui_str(UI_STR_NAV_HOME), ui_str(UI_STR_HOME_PLUS_PLACEHOLDER));
}

/** CJK font: LV_FONT_DEFAULT is Montserrat; set before set_text for correct UTF-8 measure. */
static void ui_home_dropdown_label_apply_cjk_font(lv_obj_t *lab)
{
    if (!lab) {
        return;
    }
    const lv_font_t *f = ui_font_cjk_always();
    lv_obj_set_style_text_font(lab, f, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lab, f, LV_PART_MAIN | LV_STATE_PRESSED);
}

/** Dropdown rows: LONG_CLIP avoids LONG_DOT mutating label->text (can break UTF-8). */
static void ui_home_dropdown_style_row_label(lv_obj_t *lab)
{
    if (!lab) {
        return;
    }
    lv_obj_set_style_text_color(lab, lv_color_hex(0x1E293B), 0);
    lv_label_set_long_mode(lab, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(lab, lv_pct(100));
    lv_obj_set_style_text_align(lab, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lab);
    ui_home_dropdown_label_apply_cjk_font(lab);
}

/* Hide the Welcome home dropdown and refresh home name */
static void ui_home_dropdown_close(void)
{
    if (s_home_dropdown_panel) {
        lv_obj_add_flag(s_home_dropdown_panel, LV_OBJ_FLAG_HIDDEN);
    }
#ifndef RM_HOST_BUILD
    printf("heap size: %ld, minimum heap size: %ld\n", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
#endif
}

/* Dropdown list item clicked: user_data is group_name from backend */
static void ui_home_dropdown_item_click_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    const char *group_name = (const char *)lv_event_get_user_data(e);
    if (group_name != NULL) {
        rm_app_backend_set_current_group_by_name(group_name);
        ui_Screen_Home_set_home_name(rm_app_backend_get_current_group_name());
        ui_Screen_Home_set_devices(rm_app_backend_get_current_home_devices_count());
    }
    ui_home_dropdown_close();
}

static void ui_home_dropdown_selector_click_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (s_home_dropdown_panel == NULL) {
        return;
    }
    if (lv_obj_has_flag(s_home_dropdown_panel, LV_OBJ_FLAG_HIDDEN)) {
        /* Build list and show */
        lv_obj_clean(s_home_dropdown_panel);
        lv_obj_update_layout(s_home_dropdown_panel);
        int count = rm_app_backend_get_groups_count();
        int y_off = 0;
        const int row_h = ui_display_scale_px(36);
        for (int i = 0; i < count; i++) {
            rm_app_group_t group;
            if (!rm_app_backend_get_group_by_index(i, &group) || !group.group_name || group.group_name[0] == '\0') {
                continue;
            }
            lv_obj_t *row = lv_obj_create(s_home_dropdown_panel);
            lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_size(row, lv_pct(100), row_h - 4);
            lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, y_off);
            lv_obj_set_style_radius(row, ui_display_scale_px(10), 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_bg_color(row, lv_color_hex(0xF5F8FF), 0);
            lv_obj_set_style_bg_color(row, lv_color_hex(0xE0E8F5), LV_STATE_PRESSED);
            lv_obj_set_style_pad_all(row, 0, 0);
            lv_obj_set_style_pad_left(row, ui_display_scale_px(10), 0);
            lv_obj_set_style_pad_right(row, ui_display_scale_px(10), 0);
            lv_obj_t *lab = lv_label_create(row);
            lv_obj_remove_style_all(lab);
            ui_home_dropdown_label_apply_cjk_font(lab);
            lv_label_set_text(lab, group.group_name);
            ui_home_dropdown_style_row_label(lab);
            lv_obj_add_event_cb(row, ui_home_dropdown_item_click_cb, LV_EVENT_CLICKED, (void *)group.group_name);
            y_off += row_h;
        }
        lv_obj_set_height(s_home_dropdown_panel, (int32_t)y_off + 8);
        lv_obj_clear_flag(s_home_dropdown_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_home_dropdown_panel);
    } else {
        ui_home_dropdown_close();
    }
}

static lv_obj_t *ui_home_create_top_bar(lv_obj_t *parent)
{
    const int32_t bar_h = ui_display_top_bar_height();
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(bar, lv_pct(100));
    lv_obj_set_height(bar, bar_h);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xF5F8FF), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_left(bar, ui_display_scale_px(18), 0);
    lv_obj_set_style_pad_right(bar, ui_display_scale_px(18), 0);
    lv_obj_set_style_pad_top(bar, ui_display_scale_px(14), 0);
    lv_obj_set_style_pad_bottom(bar, ui_display_scale_px(10), 0);

    lv_obj_t *title = lv_label_create(bar);
    s_home_title_lab = title;
    lv_label_set_text(title, ui_str(UI_STR_MY_DEVICES));
    lv_obj_set_style_text_font(title, ui_font_title(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, ui_display_scale_px(4));

    /* Clickable plus button (top-right) */
    lv_obj_t *plus_btn = lv_btn_create(bar);
    lv_obj_clear_flag(plus_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(plus_btn, ui_display_scale_px(40), ui_display_scale_px(40));
    lv_obj_align(plus_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_opa(plus_btn, LV_OPA_0, 0);
    lv_obj_set_style_border_width(plus_btn, 0, 0);
    lv_obj_set_style_shadow_width(plus_btn, 0, 0);
    lv_obj_add_event_cb(plus_btn, ui_home_plus_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *plus = lv_label_create(plus_btn);
    lv_label_set_text(plus, "+");
    lv_obj_set_style_text_font(plus, ui_font_title(), 0);
    lv_obj_set_style_text_color(plus, lv_color_hex(0x2E6BE6), 0);
    lv_obj_center(plus);

    return bar;
}

static lv_obj_t *ui_home_create_welcome_card(lv_obj_t *parent)
{
    const int32_t welcome_top = ui_display_scale_px(60);
    const lv_font_t *f_welcome = ui_font_welcome_card();
    const int32_t lh = lv_font_get_line_height(f_welcome);
    const int32_t pad = ui_display_scale_px(10);
    const int32_t gap_lines = ui_display_scale_px(4);
    const int32_t selector_h = LV_MAX(ui_display_scale_px(26), lh + ui_display_scale_px(6));
    /* Design 55px was too short when scaled to ~28px on 320×240 — two text rows need real height */
    const int32_t min_h = pad * 2 + lh + gap_lines + selector_h;
    const int32_t card_h = LV_MAX(ui_display_scale_px(55), min_h);

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(card, lv_pct(92));
    lv_obj_set_height(card, card_h);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, welcome_top);
    lv_obj_set_style_radius(card, ui_display_scale_px(18), 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0xB0C4EE), 0);
    lv_obj_set_style_shadow_width(card, ui_display_scale_px(18), 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_20, 0);
    lv_obj_set_style_pad_all(card, pad, 0);

    lv_obj_t *welcome = lv_label_create(card);
    s_home_welcome_lab = welcome;
    lv_obj_remove_style_all(welcome);
    lv_obj_set_style_text_font(welcome, f_welcome, 0);
    lv_label_set_text(welcome, ui_str(UI_STR_WELCOME_TO));
    lv_label_set_long_mode(welcome, LV_LABEL_LONG_DOT);
    lv_obj_set_width(welcome, lv_pct(48));
    lv_obj_align(welcome, LV_ALIGN_TOP_LEFT, ui_display_scale_px(2), pad);

    /* Clickable selector: plain obj (not lv_btn — theme can force Montserrat on child labels). */
    s_home_group_selector_btn = lv_obj_create(card);
    lv_obj_add_flag(s_home_group_selector_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_home_group_selector_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(s_home_group_selector_btn, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_home_group_selector_btn, 0, 0);
    lv_obj_set_style_shadow_width(s_home_group_selector_btn, 0, 0);
    lv_obj_set_height(s_home_group_selector_btn, selector_h);
    lv_obj_set_width(s_home_group_selector_btn, ui_display_scale_px(140));
    lv_obj_align_to(s_home_group_selector_btn, welcome, LV_ALIGN_OUT_BOTTOM_LEFT, 0, gap_lines);
    lv_obj_add_event_cb(s_home_group_selector_btn, ui_home_dropdown_selector_click_cb, LV_EVENT_CLICKED, NULL);

    ui_Home_Label_HomeName = lv_label_create(s_home_group_selector_btn);
    lv_obj_remove_style_all(ui_Home_Label_HomeName);
    lv_obj_set_style_text_font(ui_Home_Label_HomeName, f_welcome, 0);
    lv_label_set_text(ui_Home_Label_HomeName, ui_str(UI_STR_HOME_DEFAULT));
    lv_obj_set_style_text_color(ui_Home_Label_HomeName, lv_color_hex(0x2E6BE6), 0);
    lv_obj_set_width(ui_Home_Label_HomeName, lv_pct(78));
    lv_label_set_long_mode(ui_Home_Label_HomeName, LV_LABEL_LONG_CLIP);
    lv_obj_align(ui_Home_Label_HomeName, LV_ALIGN_LEFT_MID, ui_display_scale_px(2), 0);

    lv_obj_t *arrow = lv_label_create(s_home_group_selector_btn);
    lv_obj_remove_style_all(arrow);
    lv_obj_set_style_text_font(arrow, ui_font_montserrat_16(), 0);
    lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(arrow, lv_color_hex(0x8AA0C8), 0);
    lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, 0, 0);

    /* Right aligned PNG image */
    lv_obj_t *home_img = lv_img_create(card);
    lv_img_set_src(home_img, &ui_img_home_png);
    lv_obj_align(home_img, LV_ALIGN_RIGHT_MID, ui_display_scale_px(-2), 0);

    return card;
}

static int32_t ui_home_create_common_header(lv_obj_t *parent, lv_obj_t *welcome_card)
{
    const int32_t gap = ui_display_scale_px(8);
    int32_t y = ui_display_scale_px(130);
    if (welcome_card) {
        y = lv_obj_get_y(welcome_card) + lv_obj_get_height(welcome_card) + gap;
    }

    lv_obj_t *lab = lv_label_create(parent);
    s_home_common_lab = lab;
    lv_label_set_text(lab, ui_str(UI_STR_COMMON));
    lv_obj_set_style_text_font(lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(lab, lv_color_hex(0x2E6BE6), 0);
    lv_obj_align(lab, LV_ALIGN_TOP_LEFT, ui_display_scale_px(18), y);

    /* Refresh button (right) */
    s_home_common_refresh_btn = lv_btn_create(parent);
    lv_obj_clear_flag(s_home_common_refresh_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_home_common_refresh_btn, ui_display_scale_px(28), ui_display_scale_px(28));
    lv_obj_align(s_home_common_refresh_btn, LV_ALIGN_TOP_RIGHT, ui_display_scale_px(-18), y - ui_display_scale_px(4));
    lv_obj_set_style_bg_opa(s_home_common_refresh_btn, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_home_common_refresh_btn, 0, 0);
    lv_obj_set_style_shadow_width(s_home_common_refresh_btn, 0, 0);
    lv_obj_add_event_cb(s_home_common_refresh_btn, ui_home_common_refresh_btn_event_cb, LV_EVENT_CLICKED, NULL);

    s_home_common_refresh_icon = lv_label_create(s_home_common_refresh_btn);
    lv_label_set_text(s_home_common_refresh_icon, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_font(s_home_common_refresh_icon, ui_font_montserrat_18(), 0);
    lv_obj_set_style_text_color(s_home_common_refresh_icon, lv_color_hex(0x2E6BE6), 0);
    lv_obj_center(s_home_common_refresh_icon);

    /* LVGL 8: lv_spinner_create(parent, time_ms, arc_length); LVGL 9: create + set_anim_params */
#if LVGL_VERSION_MAJOR >= 9
    s_home_common_refresh_spinner = lv_spinner_create(s_home_common_refresh_btn);
    if (s_home_common_refresh_spinner) {
        lv_spinner_set_anim_params(s_home_common_refresh_spinner, 650, 120);
    }
#else
    s_home_common_refresh_spinner = lv_spinner_create(s_home_common_refresh_btn, 650, 120);
#endif
    lv_obj_set_size(s_home_common_refresh_spinner, ui_display_scale_px(14), ui_display_scale_px(14));
    lv_obj_set_style_arc_width(s_home_common_refresh_spinner, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_home_common_refresh_spinner, 2, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_home_common_refresh_spinner, lv_color_hex(0xD7E1F5), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_home_common_refresh_spinner, lv_color_hex(0x2E6BE6), LV_PART_INDICATOR);
    lv_obj_center(s_home_common_refresh_spinner);
    lv_obj_add_flag(s_home_common_refresh_spinner, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(line, ui_display_scale_px(110), ui_display_scale_px(4));
    lv_obj_set_style_bg_color(line, lv_color_hex(0x2E6BE6), 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_radius(line, ui_display_scale_px(2), 0);
    lv_obj_align(line, LV_ALIGN_TOP_LEFT, ui_display_scale_px(18), y + ui_display_scale_px(30));

    /* Grid should sit close to the Common divider line */
    return (y + ui_display_scale_px(30) + gap);
}

static lv_obj_t *ui_home_create_device_grid(lv_obj_t *parent, int32_t top_y, int32_t height)
{
    ui_home_compute_card_dims();
    ui_Home_Cont_DeviceGrid = lv_obj_create(parent);
    /* Allow scrolling when there are more devices */
    lv_obj_add_flag(ui_Home_Cont_DeviceGrid, LV_OBJ_FLAG_SCROLLABLE);
    /* Allow swipe gesture to bubble up to the screen for page switching */
    lv_obj_add_flag(ui_Home_Cont_DeviceGrid, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_width(ui_Home_Cont_DeviceGrid, lv_pct(92));
    if (height < ui_display_scale_px(120)) {
        height = ui_display_scale_px(120);
    }
    lv_obj_set_height(ui_Home_Cont_DeviceGrid, height);
    /* Place grid right under the Common divider (minimal whitespace) */
    lv_obj_align(ui_Home_Cont_DeviceGrid, LV_ALIGN_TOP_MID, 0, top_y);
    lv_obj_set_style_bg_opa(ui_Home_Cont_DeviceGrid, LV_OPA_0, 0);
    lv_obj_set_style_border_width(ui_Home_Cont_DeviceGrid, 0, 0);
    lv_obj_set_style_pad_all(ui_Home_Cont_DeviceGrid, 0, 0);
    lv_obj_set_style_pad_row(ui_Home_Cont_DeviceGrid, ui_display_scale_px(16), 0);
    lv_obj_set_style_pad_column(ui_Home_Cont_DeviceGrid, ui_display_scale_px(10), 0);

    lv_obj_set_flex_flow(ui_Home_Cont_DeviceGrid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(ui_Home_Cont_DeviceGrid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    return ui_Home_Cont_DeviceGrid;
}

static void ui_home_power_ctx_delete_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }
    ui_home_power_ctx_t *ctx = (ui_home_power_ctx_t *)lv_event_get_user_data(e);
    if (ctx) {
        UI_LV_FREE(ctx);
    }
}

static void ui_home_card_ctx_delete_cb(lv_event_t *e)
{
    ui_home_card_ctx_t *ctx = (ui_home_card_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) {
        return;
    }
    ui_home_card_ctx_t **pp = &s_home_card_ctx_head;
    while (*pp) {
        if (*pp == ctx) {
            *pp = ctx->next;
            break;
        }
        pp = &(*pp)->next;
    }
    UI_LV_FREE(ctx);
}

static void ui_home_update_card_from_dev(ui_home_card_ctx_t *ctx, const rm_app_device_t *dev)
{
    if (!ctx || !dev) {
        return;
    }
    if (!dev->online) {
        lv_obj_set_style_bg_color(ctx->card, lv_color_hex(0xDDE6F6), 0);
        lv_obj_set_style_bg_opa(ctx->card, LV_OPA_80, 0);
    } else {
        lv_obj_set_style_bg_color(ctx->card, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(ctx->card, LV_OPA_COVER, 0);
    }
    if (ctx->name) {
        lv_label_set_text(ctx->name, ui_device_name_for_display(dev->name && dev->name[0] ? dev->name : ""));
    }
    if (ctx->icon) {
#if LVGL_VERSION_MAJOR >= 9
        lv_img_set_src(ctx->icon, rainmaker_standard_device_get_device_icon_src(dev->type, dev->power_on));
#else
        lv_img_set_src(ctx->icon, rainmaker_standard_device_get_device_icon_src(dev->type, dev->power_on));
#endif
    }
    if (ctx->sw) {
        if (dev->power_on) {
            lv_obj_add_state(ctx->sw, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(ctx->sw, LV_STATE_CHECKED);
        }
        if (dev->online) {
            lv_obj_add_flag(ctx->sw, LV_OBJ_FLAG_CLICKABLE);
        } else {
            lv_obj_clear_flag(ctx->sw, LV_OBJ_FLAG_CLICKABLE);
        }
    }
}

static void ui_home_power_switch_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    lv_obj_t *sw = lv_event_get_target(e);
    ui_home_power_ctx_t *ctx = (ui_home_power_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || !ctx->device) {
        return;
    }

    /* Device offline: do not respond, rollback switch state, no msgbox */
    if (!((rm_device_item_t *)ctx->device)->online) {
        bool power_on = lv_obj_has_state(sw, LV_STATE_CHECKED);
        if (power_on) {
            lv_obj_clear_state(sw, LV_STATE_CHECKED);
        } else {
            lv_obj_add_state(sw, LV_STATE_CHECKED);
        }
        return;
    }

    bool power_on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    const char *err = NULL;
    esp_err_t ret = rainmaker_standard_device_set_power((rm_device_item_t *)ctx->device, power_on, &err);
    if (ret != ESP_OK) {
        /* Rollback UI state */
        if (power_on) {
            lv_obj_clear_state(sw, LV_STATE_CHECKED);
        } else {
            lv_obj_add_state(sw, LV_STATE_CHECKED);
        }
        ui_show_msgbox(ui_str(UI_STR_POWER), (err && err[0]) ? err : ui_str(UI_STR_FAILED_SET_POWER));
    } else {
        /* Update icon immediately */
        if (ctx->icon) {
            rm_app_device_t dev = {
                .name = ctx->device->name,
                .online = ctx->device->online,
                .whether_has_power = ctx->device->whether_has_power,
                .power_on = ctx->device->power_on,
                .type = ctx->device->type,
            };
#if LVGL_VERSION_MAJOR >= 9
            lv_img_set_src(ctx->icon, rainmaker_standard_device_get_device_icon_src(dev.type, dev.power_on));
#else
            lv_img_set_src(ctx->icon, rainmaker_standard_device_get_device_icon_src(dev.type, dev.power_on));
#endif
        }
    }

    lv_event_stop_bubbling(e);
}

static void ui_home_style_power_switch(lv_obj_t *sw)
{
    /* Match the app-like pill switch style in the reference screenshot */
    if (!sw) {
        return;
    }

    /* Scale with card; keep pill proportion vs 132px card width */
    int32_t track_w = (s_home_card_w * 56) / 132;
    int32_t track_h = (s_home_card_w * 26) / 132;
    if (track_w < 44) {
        track_w = ui_display_scale_px(56);
        track_h = ui_display_scale_px(26);
    }
    lv_obj_set_size(sw, track_w, track_h);
    const int32_t r_main = LV_MAX(8, track_h / 2);
    lv_obj_set_style_radius(sw, r_main, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(sw, r_main, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    /* Remove borders/shadows */
    lv_obj_set_style_border_width(sw, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(sw, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(sw, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* Track colors */
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0xE8EEF8), LV_PART_MAIN | LV_STATE_DEFAULT); /* off track */

    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0xCFE0FF), LV_PART_INDICATOR | LV_STATE_CHECKED); /* on track */

    /* Knob */
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0xAFC0E6), LV_PART_KNOB | LV_STATE_DEFAULT); /* off knob */
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x2E6BE6), LV_PART_KNOB | LV_STATE_CHECKED); /* on knob */

    lv_obj_set_style_border_width(sw, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(sw, LV_MAX(8, track_h / 2 - 2), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(sw, ui_display_scale_px(2), LV_PART_MAIN | LV_STATE_DEFAULT);
    const int32_t knob = LV_MAX(ui_display_scale_px(18), track_h - ui_display_scale_px(4));
    lv_obj_set_style_width(sw, knob, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_height(sw, knob, LV_PART_KNOB | LV_STATE_DEFAULT);
}

static void ui_home_device_card_click_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    /* Only open detail when the card itself was clicked (not the power switch child) */
    if (lv_event_get_target(e) != lv_event_get_current_target(e)) {
        return;
    }
    rm_device_item_handle_t handle = (rm_device_item_handle_t)lv_event_get_user_data(e);
    if (handle == NULL) {
        return;
    }

    rm_app_device_type_t type = rm_app_backend_get_device_type(handle);
    if (type == RAINMAKER_APP_DEVICE_TYPE_LIGHT) {
        ui_Screen_LightDetail_show(handle);
    } else if (type == RAINMAKER_APP_DEVICE_TYPE_FAN) {
        ui_Screen_FanDetails_show(handle);
    } else if (type == RAINMAKER_APP_DEVICE_TYPE_SWITCH) {
        ui_Screen_SwitchDetails_show(handle);
    } else {
        /* Not supported device type, only show the error */
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_UNSUPPORTED_DEVICE));
    }
}

static void ui_home_add_device_card(lv_obj_t *grid, const rm_app_device_t *dev, rm_device_item_handle_t device)
{
    lv_obj_t *card = lv_btn_create(grid);
    if (card == NULL) {
        return;
    }
    /* Width/height from ui_home_compute_card_dims() — 3–4 columns on P4 1024-wide */
    lv_obj_set_size(card, s_home_card_w, s_home_card_h);
    lv_obj_set_style_radius(card, ui_display_scale_px(18), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_shadow_width(card, ui_display_scale_px(18), 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0xB0C4EE), 0);
    /* LVGL provides LV_OPA_0/10/20/...; use the closest value */
    lv_obj_set_style_shadow_opa(card, LV_OPA_20, 0);
    lv_obj_set_style_pad_all(card, ui_display_scale_px(10), 0);

    if (!dev->online) {
        lv_obj_set_style_bg_color(card, lv_color_hex(0xDDE6F6), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_80, 0);
    } else {
        lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
    }

    lv_obj_add_event_cb(card, ui_home_device_card_click_cb, LV_EVENT_CLICKED, (void *)device);

    /* Name first (bottom), then icon: icon stays above label in z-order if regions touch.
     * LONG_CLIP avoids LONG_DOT mutating UTF-8 in the label buffer. */
    lv_obj_t *name = lv_label_create(card);
    lv_label_set_text(name, ui_device_name_for_display(dev->name && dev->name[0] ? dev->name : ""));
    lv_obj_set_style_text_font(name, ui_i18n_uses_cjk() ? ui_font_cjk_always() : ui_font_body(), 0);
    lv_obj_set_width(name, lv_pct(100));
    lv_label_set_long_mode(name, LV_LABEL_LONG_CLIP);
    lv_obj_align(name, LV_ALIGN_BOTTOM_LEFT, 0, 0);

#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t *icon = lv_img_create(card);
    lv_img_set_src(icon, rainmaker_standard_device_get_device_icon_src(dev->type, dev->power_on));
#else
    lv_obj_t *icon = lv_img_create(card);
    lv_img_set_src(icon, rainmaker_standard_device_get_device_icon_src(dev->type, dev->power_on));
#endif
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(icon, LV_ALIGN_TOP_LEFT, 0, 0);

    ui_home_card_ctx_t *card_ctx = (ui_home_card_ctx_t *)UI_LV_MALLOC(sizeof(ui_home_card_ctx_t));
    if (card_ctx) {
        card_ctx->handle = device;
        card_ctx->card = card;
        card_ctx->icon = icon;
        card_ctx->name = name;
        card_ctx->sw = NULL;
        card_ctx->next = s_home_card_ctx_head;
        s_home_card_ctx_head = card_ctx;
        lv_obj_add_event_cb(card, ui_home_card_ctx_delete_cb, LV_EVENT_DELETE, card_ctx);
    }

    /* Power toggle (right top). Only show if device has power switch. */
    if (dev && dev->whether_has_power) {
        lv_obj_t *sw = lv_switch_create(card);
        lv_obj_clear_flag(sw, LV_OBJ_FLAG_SCROLLABLE);
        ui_home_style_power_switch(sw);
        lv_obj_align(sw, LV_ALIGN_TOP_RIGHT, 0, 0);

        if (dev->power_on) {
            lv_obj_add_state(sw, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(sw, LV_STATE_CHECKED);
        }

        ui_home_power_ctx_t *ctx = (ui_home_power_ctx_t *)UI_LV_MALLOC(sizeof(ui_home_power_ctx_t));
        if (ctx) {
            ctx->device = device;
            ctx->icon = icon;
            lv_obj_add_event_cb(sw, ui_home_power_ctx_delete_cb, LV_EVENT_DELETE, ctx);
            lv_obj_add_event_cb(sw, ui_home_power_switch_event_cb, LV_EVENT_VALUE_CHANGED, ctx);
        }
        if (card_ctx) {
            card_ctx->sw = sw;
        }
        if (!dev->online) {
            lv_obj_clear_flag(sw, LV_OBJ_FLAG_CLICKABLE);
        }
    }
}

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

static void ui_Screen_Home_set_devices(int device_count)
{
    if (ui_Home_Cont_DeviceGrid == NULL) {
        return;
    }

    lv_obj_clean(ui_Home_Cont_DeviceGrid);
    for (int i = 0; i < device_count; i++) {
        rm_app_device_t device;
        rm_device_item_handle_t handle = rm_app_backend_get_current_home_device_by_index(i, &device);
        if (handle) {
            ui_home_add_device_card(ui_Home_Cont_DeviceGrid, &device, handle);
        }
    }
}

void ui_Screen_Home_reload_device_cards(void)
{
    if (ui_Home_Cont_DeviceGrid == NULL) {
        return;
    }
    int devs_count = rm_app_backend_get_current_home_devices_count();
    ui_Screen_Home_set_devices(devs_count);
}

void ui_Screen_Home_apply_language(void)
{
    if (s_home_title_lab) {
        lv_label_set_text(s_home_title_lab, ui_str(UI_STR_MY_DEVICES));
        lv_obj_set_style_text_font(s_home_title_lab, ui_font_title(), 0);
    }
    if (s_home_welcome_lab) {
        lv_obj_set_style_text_font(s_home_welcome_lab, ui_font_welcome_card(), 0);
        lv_label_set_text(s_home_welcome_lab, ui_str(UI_STR_WELCOME_TO));
    }
    if (s_home_common_lab) {
        lv_label_set_text(s_home_common_lab, ui_str(UI_STR_COMMON));
        lv_obj_set_style_text_font(s_home_common_lab, ui_font_body(), 0);
    }
    if (ui_Home_Label_HomeName) {
        const char *gn = rm_app_backend_get_current_group_name();
        lv_obj_set_style_text_font(ui_Home_Label_HomeName, ui_font_welcome_card(), 0);
        if (gn && gn[0]) {
            lv_label_set_text(ui_Home_Label_HomeName, gn);
        } else {
            lv_label_set_text(ui_Home_Label_HomeName, ui_str(UI_STR_HOME_DEFAULT));
        }
    }
    for (ui_home_card_ctx_t *ctx = s_home_card_ctx_head; ctx; ctx = ctx->next) {
        if (!ctx->name || !ctx->handle) {
            continue;
        }
        rm_app_device_t dev;
        if (!rm_app_backend_get_device_info_by_handle(ctx->handle, &dev)) {
            continue;
        }
#if !defined(RM_HOST_BUILD)
        const char *raw_name = NULL;
        if (dev.name && esp_ptr_byte_accessible(dev.name) && dev.name[0]) {
            raw_name = dev.name;
        }
#else
        const char *raw_name = (dev.name && dev.name[0]) ? dev.name : NULL;
#endif
        lv_label_set_text(ctx->name, raw_name ? ui_device_name_for_display(raw_name) : ui_str(UI_STR_DEVICE));
        lv_obj_set_style_text_font(ctx->name, ui_i18n_uses_cjk() ? ui_font_cjk_always() : ui_font_body(), 0);
    }
}

void ui_Screen_Home_refresh_from_backend(void)
{
    bool ok = rm_app_backend_refresh_home_screen(true);
    ui_hide_busy();
    if (!ok) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_FAILED_REFRESH_HOME));
        return;
    } else {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_SUCCESS), ui_str(UI_STR_HOME_REFRESH_OK));
    }

    ui_Screen_Home_set_home_name(rm_app_backend_get_current_group_name());
    int devs_count = rm_app_backend_get_current_home_devices_count();
    if (devs_count <= 0) {
        return;
    }
    ui_Screen_Home_set_devices(devs_count);
}

void ui_Screen_Home_update_device_card(rm_device_item_handle_t device)
{
    if (!device) {
        return;
    }
    rm_device_item_t *dev_item = (rm_device_item_t *)device;
    rm_app_device_t dev = {
        .name = dev_item->name,
        .online = dev_item->online,
        .whether_has_power = dev_item->whether_has_power,
        .power_on = dev_item->power_on,
        .type = dev_item->type,
    };
    for (ui_home_card_ctx_t *ctx = s_home_card_ctx_head; ctx; ctx = ctx->next) {
        if (ctx->handle == device) {
            ui_home_update_card_from_dev(ctx, &dev);
            break;
        }
    }
}

void ui_Screen_Home_screen_init(void)
{
    ui_Screen_Home = lv_obj_create(NULL);
    /* Enable vertical scroll to reveal all content */
    lv_obj_add_flag(ui_Screen_Home, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(ui_Screen_Home, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(ui_Screen_Home, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(ui_Screen_Home, lv_color_hex(0xF5F8FF), 0);
    ui_gesture_nav_enable(ui_Screen_Home);

    (void)ui_home_create_top_bar(ui_Screen_Home);

    lv_obj_t *welcome_card = ui_home_create_welcome_card(ui_Screen_Home);
    lv_obj_update_layout(ui_Screen_Home);
    /* Dropdown panel for "Home >" (below welcome card, overlapping content) */
    s_home_dropdown_panel = lv_obj_create(ui_Screen_Home);
    lv_obj_clear_flag(s_home_dropdown_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(s_home_dropdown_panel, lv_pct(92));
    lv_obj_set_height(s_home_dropdown_panel, ui_display_scale_px(120));
    lv_obj_align(s_home_dropdown_panel, LV_ALIGN_TOP_MID, 0,
                 lv_obj_get_y(welcome_card) + lv_obj_get_height(welcome_card) + ui_display_scale_px(4));
    lv_obj_set_style_radius(s_home_dropdown_panel, ui_display_scale_px(14), 0);
    lv_obj_set_style_bg_color(s_home_dropdown_panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(s_home_dropdown_panel, 0, 0);
    lv_obj_set_style_shadow_width(s_home_dropdown_panel, ui_display_scale_px(12), 0);
    lv_obj_set_style_shadow_color(s_home_dropdown_panel, lv_color_hex(0xB0C4EE), 0);
    lv_obj_set_style_pad_all(s_home_dropdown_panel, ui_display_scale_px(6), 0);
    lv_obj_add_flag(s_home_dropdown_panel, LV_OBJ_FLAG_HIDDEN);

    int32_t grid_top_y = ui_home_create_common_header(ui_Screen_Home, welcome_card);
    /* Nav is hidden by default and shown as an overlay; do not reserve its height */
    int32_t yres = ui_display_get_ver_res();
    if (yres <= 0) {
        yres = 240;
    }
    int32_t grid_h = yres - grid_top_y - ui_display_scale_px(12);
    (void)ui_home_create_device_grid(ui_Screen_Home, grid_top_y, grid_h);

    /* Bottom nav removed: swipe left/right to switch screens */
}

void ui_Screen_Home_screen_destroy(void)
{
    if (ui_Screen_Home) {
        lv_obj_del(ui_Screen_Home);
    }

    ui_Screen_Home = NULL;
    ui_Home_Label_HomeName = NULL;
    ui_Home_Cont_DeviceGrid = NULL;
    s_home_refreshing = false;
    s_home_common_refresh_btn = NULL;
    s_home_common_refresh_icon = NULL;
    s_home_common_refresh_spinner = NULL;
    s_home_dropdown_panel = NULL;
    s_home_group_selector_btn = NULL;
    s_home_title_lab = NULL;
    s_home_welcome_lab = NULL;
    s_home_common_lab = NULL;
}
