/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Fan device detail screen: power (same as Light) and speed slider 0-5 (UI like brightness).
 */

#include "../ui.h"
#include "rainmaker_app_backend.h"
#include "rainmaker_device_group_manager.h"
#include "rainmaker_standard_device.h"
#include <stdio.h>
#ifndef RM_HOST_BUILD
#include "esp_memory_utils.h"
#endif

lv_obj_t *ui_Screen_FanDetails = NULL;

static rm_device_item_handle_t s_fan_device = NULL;
static lv_obj_t *s_title_label = NULL;
static lv_obj_t *s_speed_caption_lab = NULL;
static lv_obj_t *s_power_btn = NULL;
static lv_obj_t *s_speed_slider = NULL;
static lv_obj_t *s_speed_value_label = NULL;

#define UI_FAN_DETAIL_SLIDER_TRACK_H        6
#define UI_FAN_DETAIL_SLIDER_EXT_CLICK_PAD  32
#define UI_FAN_DETAIL_SLIDER_OBJ_H          12
#define UI_FAN_DETAIL_SLIDER_KNOB_D         30
#define UI_FAN_DETAIL_SLIDER_KNOB_PAD       ((UI_FAN_DETAIL_SLIDER_KNOB_D - UI_FAN_DETAIL_SLIDER_OBJ_H) / 2)
#define UI_FAN_DETAIL_LABEL_ROW_H           18
#define UI_FAN_DETAIL_SPEED_MIN             0
#define UI_FAN_DETAIL_SPEED_MAX             5

static void ui_fan_detail_style_slider_knob_only(lv_obj_t *slider)
{
    if (!slider) {
        return;
    }
    lv_obj_add_flag(slider, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_add_flag(slider, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_ext_click_area(slider, UI_FAN_DETAIL_SLIDER_EXT_CLICK_PAD);
    lv_obj_set_style_bg_opa(slider, LV_OPA_0, 0);

    lv_obj_set_style_height(slider, UI_FAN_DETAIL_SLIDER_TRACK_H, LV_PART_MAIN);
    lv_obj_set_style_height(slider, UI_FAN_DETAIL_SLIDER_TRACK_H, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, UI_FAN_DETAIL_SLIDER_TRACK_H / 2, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, UI_FAN_DETAIL_SLIDER_TRACK_H / 2, LV_PART_INDICATOR);
    const int32_t knob_half = UI_FAN_DETAIL_SLIDER_KNOB_D / 2;
    lv_obj_set_style_pad_left(slider, knob_half, LV_PART_MAIN);
    lv_obj_set_style_pad_right(slider, knob_half, LV_PART_MAIN);
    lv_obj_set_style_pad_left(slider, knob_half, LV_PART_INDICATOR);
    lv_obj_set_style_pad_right(slider, knob_half, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xE2E8F0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x2E6BE6), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_90, LV_PART_INDICATOR);

    lv_obj_set_style_width(slider, UI_FAN_DETAIL_SLIDER_KNOB_D, LV_PART_KNOB);
    lv_obj_set_style_height(slider, UI_FAN_DETAIL_SLIDER_KNOB_D, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, UI_FAN_DETAIL_SLIDER_KNOB_PAD, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_border_width(slider, 2, LV_PART_KNOB);
    lv_obj_set_style_border_color(slider, lv_color_hex(0xD0D7E2), LV_PART_KNOB);
    lv_obj_set_style_shadow_width(slider, 6, LV_PART_KNOB);
    lv_obj_set_style_shadow_color(slider, lv_color_hex(0x000000), LV_PART_KNOB);
    lv_obj_set_style_shadow_opa(slider, LV_OPA_10, LV_PART_KNOB);
}

static void ui_fan_detail_refresh_from_device(void)
{
    /* No bound device (e.g. PC visual tests): keep title/caption in sync with current
     * language when ui_Screen_FanDetails_apply_language() switches fonts — same as Switch. */
    if (!s_fan_device) {
        if (s_title_label) {
            lv_label_set_text(s_title_label, ui_str(UI_STR_FAN));
            lv_obj_set_style_text_font(s_title_label, ui_i18n_uses_cjk() ? ui_font_cjk_always() : ui_font_title(), 0);
        }
        if (s_power_btn) {
            lv_obj_set_style_bg_color(s_power_btn, lv_color_hex(0xA0B0C8), 0);
        }
        if (s_speed_slider) {
            lv_slider_set_value(s_speed_slider, UI_FAN_DETAIL_SPEED_MIN, LV_ANIM_OFF);
            lv_obj_add_flag(s_speed_slider, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(s_speed_slider, lv_color_hex(0x2E6BE6), LV_PART_MAIN);
            lv_obj_set_style_bg_color(s_speed_slider, lv_color_hex(0x2E6BE6), LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(s_speed_slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
        }
        if (s_speed_value_label) {
            lv_label_set_text(s_speed_value_label, "0");
        }
        return;
    }
    rm_device_item_t *dev = (rm_device_item_t *)s_fan_device;
    if (s_title_label) {
#if !defined(RM_HOST_BUILD)
        const char *raw_title = NULL;
        if (dev->name && esp_ptr_byte_accessible((void *)dev->name) && dev->name[0]) {
            raw_title = dev->name;
        }
#else
        const char *raw_title = (dev->name && dev->name[0]) ? dev->name : NULL;
#endif
        if (raw_title) {
            lv_label_set_text(s_title_label, ui_device_name_for_display(raw_title));
        } else {
            lv_label_set_text(s_title_label, ui_str(UI_STR_FAN));
        }
        lv_obj_set_style_text_font(s_title_label, ui_i18n_uses_cjk() ? ui_font_cjk_always() : ui_font_title(), 0);
    }
    if (s_power_btn) {
        if (dev->power_on) {
            lv_obj_set_style_bg_color(s_power_btn, lv_color_hex(0x2E6BE6), 0);
        } else {
            lv_obj_set_style_bg_color(s_power_btn, lv_color_hex(0xA0B0C8), 0);
        }
        if (dev->online) {
            lv_obj_add_flag(s_power_btn, LV_OBJ_FLAG_CLICKABLE);
        } else {
            lv_obj_clear_flag(s_power_btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(s_power_btn, lv_color_hex(0x9CA3AF), 0);
        }
    }
    if (s_speed_slider) {
        int speed = dev->fan.speed;
        if (speed < UI_FAN_DETAIL_SPEED_MIN) {
            speed = UI_FAN_DETAIL_SPEED_MIN;
        }
        if (speed > UI_FAN_DETAIL_SPEED_MAX) {
            speed = UI_FAN_DETAIL_SPEED_MAX;
        }
        lv_slider_set_value(s_speed_slider, speed, LV_ANIM_OFF);
    }
    if (s_speed_value_label) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", (int)dev->fan.speed);
        lv_label_set_text(s_speed_value_label, buf);
    }
    if (dev->online) {
        if (s_speed_slider) {
            lv_obj_add_flag(s_speed_slider, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(s_speed_slider, lv_color_hex(0x2E6BE6), LV_PART_MAIN);
            lv_obj_set_style_bg_color(s_speed_slider, lv_color_hex(0x2E6BE6), LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(s_speed_slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
        }
    } else {
        if (s_speed_slider) {
            lv_obj_clear_flag(s_speed_slider, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(s_speed_slider, lv_color_hex(0x9CA3AF), LV_PART_MAIN);
            lv_obj_set_style_bg_color(s_speed_slider, lv_color_hex(0x9CA3AF), LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(s_speed_slider, lv_color_hex(0xC0C4CC), LV_PART_KNOB);
        }
    }
}

static void ui_fan_detail_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    extern lv_obj_t *ui_Screen_Home;
    if (ui_Screen_Home) {
        lv_scr_load(ui_Screen_Home);
        ui_Screen_Home_update_device_card(s_fan_device);
    }
}

static void ui_fan_detail_settings_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_Screen_Device_Setting_show(s_fan_device);
}

static void ui_fan_detail_power_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!s_fan_device) {
        return;
    }
    rm_device_item_t *dev = (rm_device_item_t *)s_fan_device;
    if (!dev->online) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_DEVICE_OFFLINE));
        return;
    }
    bool power_on = !dev->power_on;
    const char *err = NULL;
    if (rainmaker_standard_fan_set_power(dev->node_id, power_on, &err) != ESP_OK) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), err ? err : ui_str(UI_STR_FAILED_SET_POWER));
        return;
    }
    dev->power_on = power_on;
    ui_fan_detail_refresh_from_device();
}

static void ui_fan_detail_speed_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (!s_fan_device) {
        return;
    }
    rm_device_item_t *dev = (rm_device_item_t *)s_fan_device;
    lv_obj_t *slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);

    if (code == LV_EVENT_VALUE_CHANGED) {
        if (s_speed_value_label) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", val);
            lv_label_set_text(s_speed_value_label, buf);
        }
        return;
    }
    if (code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) {
        return;
    }
    if (!dev->online) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_DEVICE_OFFLINE));
        ui_fan_detail_refresh_from_device();
        return;
    }
    const char *err = NULL;
    if (rainmaker_standard_fan_set_speed(dev->node_id, val, &err) != ESP_OK) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), err ? err : ui_str(UI_STR_FAILED_SET_SPEED));
    } else {
        dev->fan.speed = val;
        if (s_speed_value_label) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", val);
            lv_label_set_text(s_speed_value_label, buf);
        }
    }
    ui_fan_detail_refresh_from_device();
}

static void ui_fan_detail_screen_loaded_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SCREEN_LOADED) {
        return;
    }
    ui_fan_detail_refresh_from_device();
}

static lv_obj_t *ui_fan_detail_create_header(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(bar, lv_pct(100));
    lv_obj_set_height(bar, ui_display_scale_px(56));
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xF5F8FF), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_left(bar, ui_display_scale_px(8), 0);
    lv_obj_set_style_pad_right(bar, ui_display_scale_px(8), 0);

    lv_obj_t *back = lv_btn_create(bar);
    lv_obj_clear_flag(back, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(back, ui_display_scale_px(40), ui_display_scale_px(40));
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_0, 0);
    lv_obj_set_style_border_width(back, 0, 0);
    lv_obj_add_event_cb(back, ui_fan_detail_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_arrow = lv_label_create(back);
    lv_label_set_text(back_arrow, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_arrow, lv_color_hex(0x2E6BE6), 0);
    lv_obj_center(back_arrow);

    s_title_label = lv_label_create(bar);
    lv_label_set_text(s_title_label, ui_str(UI_STR_FAN));
    lv_obj_set_style_text_font(s_title_label, ui_i18n_uses_cjk() ? ui_font_cjk_always() : ui_font_title(), 0);
    lv_obj_align(s_title_label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *settings = lv_btn_create(bar);
    lv_obj_clear_flag(settings, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(settings, ui_display_scale_px(40), ui_display_scale_px(40));
    lv_obj_align(settings, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(settings, LV_OPA_0, 0);
    lv_obj_set_style_border_width(settings, 0, 0);
    lv_obj_add_event_cb(settings, ui_fan_detail_settings_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *gear = lv_label_create(settings);
    lv_label_set_text(gear, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(gear, lv_color_hex(0x2E6BE6), 0);
    lv_obj_center(gear);

    return bar;
}

static void ui_fan_detail_create_content(lv_obj_t *parent)
{
    const int32_t pwr = ui_display_scale_px(72);
    int32_t y = ui_display_scale_px(64);  /* below header */

    /* Power button (same as Light details) */
    s_power_btn = lv_btn_create(parent);
    lv_obj_clear_flag(s_power_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_power_btn, pwr, pwr);
    lv_obj_align(s_power_btn, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_radius(s_power_btn, pwr / 2, 0);
    lv_obj_set_style_bg_color(s_power_btn, lv_color_hex(0x2E6BE6), 0);
    lv_obj_set_style_border_width(s_power_btn, 0, 0);
    lv_obj_add_event_cb(s_power_btn, ui_fan_detail_power_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *power_icon = lv_label_create(s_power_btn);
    lv_label_set_text(power_icon, LV_SYMBOL_POWER);
    lv_obj_set_style_text_color(power_icon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(power_icon, &lv_font_montserrat_20, 0);
    lv_obj_center(power_icon);
    y += pwr;

    /* Speed section (UI like brightness, range 0-5) */
    const int32_t slider_row_h = UI_FAN_DETAIL_SLIDER_KNOB_D + 8;
    const int32_t section_h = UI_FAN_DETAIL_LABEL_ROW_H + slider_row_h;
    lv_obj_t *speed_section = lv_obj_create(parent);
    lv_obj_clear_flag(speed_section, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(speed_section, lv_pct(90), section_h);
    lv_obj_align(speed_section, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_bg_opa(speed_section, LV_OPA_0, 0);
    lv_obj_set_style_border_width(speed_section, 0, 0);
    lv_obj_add_flag(speed_section, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_obj_t *label_row = lv_obj_create(speed_section);
    lv_obj_clear_flag(label_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(label_row, lv_pct(100), UI_FAN_DETAIL_LABEL_ROW_H);
    lv_obj_align(label_row, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(label_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(label_row, 0, 0);
    s_speed_caption_lab = lv_label_create(label_row);
    lv_label_set_text(s_speed_caption_lab, ui_str(UI_STR_SPEED));
    lv_obj_set_style_text_font(s_speed_caption_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(s_speed_caption_lab, lv_color_hex(0xA0AEC0), 0);
    lv_obj_align(s_speed_caption_lab, LV_ALIGN_LEFT_MID, 0, 0);
    s_speed_value_label = lv_label_create(label_row);
    lv_label_set_text(s_speed_value_label, "0");
    lv_obj_set_style_text_font(s_speed_value_label, ui_font_body(), 0);
    lv_obj_set_style_text_color(s_speed_value_label, lv_color_hex(0xA0AEC0), 0);
    lv_obj_align(s_speed_value_label, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_t *slider_row = lv_obj_create(speed_section);
    lv_obj_clear_flag(slider_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(slider_row, lv_pct(100), slider_row_h);
    lv_obj_align(slider_row, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(slider_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(slider_row, 0, 0);
    lv_obj_add_flag(slider_row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    s_speed_slider = lv_slider_create(slider_row);
    lv_obj_set_width(s_speed_slider, lv_pct(100));
    lv_obj_set_height(s_speed_slider, UI_FAN_DETAIL_SLIDER_KNOB_D);
    lv_obj_align(s_speed_slider, LV_ALIGN_CENTER, 0, 0);
    lv_slider_set_range(s_speed_slider, UI_FAN_DETAIL_SPEED_MIN, UI_FAN_DETAIL_SPEED_MAX);
    ui_fan_detail_style_slider_knob_only(s_speed_slider);
    lv_obj_add_event_cb(s_speed_slider, ui_fan_detail_speed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_speed_slider, ui_fan_detail_speed_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(s_speed_slider, ui_fan_detail_speed_cb, LV_EVENT_PRESS_LOST, NULL);
}

void ui_Screen_FanDetails_show(rm_device_item_handle_t device)
{
    if (device == NULL || ui_Screen_FanDetails == NULL) {
        return;
    }
    s_fan_device = device;
    ui_fan_detail_refresh_from_device();
    lv_scr_load(ui_Screen_FanDetails);
}

void ui_Screen_FanDetails_screen_init(void)
{
    ui_Screen_FanDetails = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_Screen_FanDetails, lv_color_hex(0xF5F8FF), 0);

    (void)ui_fan_detail_create_header(ui_Screen_FanDetails);
    ui_fan_detail_create_content(ui_Screen_FanDetails);

    lv_obj_add_event_cb(ui_Screen_FanDetails, ui_fan_detail_screen_loaded_cb, LV_EVENT_SCREEN_LOADED, NULL);
}

void ui_Screen_FanDetails_screen_destroy(void)
{
    if (ui_Screen_FanDetails) {
        lv_obj_del(ui_Screen_FanDetails);
    }
    ui_Screen_FanDetails = NULL;
    s_fan_device = NULL;
    s_title_label = NULL;
    s_speed_caption_lab = NULL;
    s_power_btn = NULL;
    s_speed_slider = NULL;
    s_speed_value_label = NULL;
}

void ui_Screen_FanDetails_apply_language(void)
{
    if (s_title_label) {
        lv_obj_set_style_text_font(s_title_label, ui_i18n_uses_cjk() ? ui_font_cjk_always() : ui_font_title(), 0);
    }
    if (s_speed_caption_lab) {
        lv_label_set_text(s_speed_caption_lab, ui_str(UI_STR_SPEED));
        lv_obj_set_style_text_font(s_speed_caption_lab, ui_font_body(), 0);
    }
    if (s_speed_value_label) {
        lv_obj_set_style_text_font(s_speed_value_label, ui_font_body(), 0);
    }
    ui_fan_detail_refresh_from_device();
}
