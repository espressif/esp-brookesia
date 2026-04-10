/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Switch device detail screen: power toggle only (large circular button + ON/OFF label).
 */

#include "../ui.h"
#include "rainmaker_app_backend.h"
#include "rainmaker_device_group_manager.h"
#include "rainmaker_standard_device.h"
#ifndef RM_HOST_BUILD
#include "esp_memory_utils.h"
#endif

lv_obj_t *ui_Screen_SwitchDetails = NULL;

static rm_device_item_handle_t s_switch_device = NULL;
static lv_obj_t *s_title_label = NULL;
static lv_obj_t *s_power_btn = NULL;
static lv_obj_t *s_status_label = NULL;

static void ui_switch_detail_refresh_from_device(void)
{
    /* No bound device (e.g. PC visual tests that only lv_scr_load this screen):
     * still sync title/status to current language. Otherwise after ui_i18n_set_lang(),
     * apply_language() switches fonts to Montserrat for EN but the label text can
     * stay as CJK from initial ui_i18n_init() — Montserrat has no those glyphs → tofu. */
    if (!s_switch_device) {
        if (s_title_label) {
            lv_label_set_text(s_title_label, ui_str(UI_STR_SWITCH));
            lv_obj_set_style_text_font(s_title_label, ui_i18n_uses_cjk() ? ui_font_cjk_always() : ui_font_title(), 0);
        }
        if (s_status_label) {
            lv_label_set_text(s_status_label, ui_str(UI_STR_OFF));
        }
        if (s_power_btn) {
            lv_obj_set_style_bg_color(s_power_btn, lv_color_hex(0xA0B0C8), 0);
        }
        return;
    }
    rm_device_item_t *dev = (rm_device_item_t *)s_switch_device;
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
            lv_label_set_text(s_title_label, ui_str(UI_STR_SWITCH));
        }
        lv_obj_set_style_text_font(s_title_label, ui_i18n_uses_cjk() ? ui_font_cjk_always() : ui_font_title(), 0);
    }
    if (s_power_btn) {
        if (dev->power_on) {
            lv_obj_set_style_bg_color(s_power_btn, lv_color_hex(0x2159C4), 0);
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
    if (s_status_label) {
        lv_label_set_text(s_status_label, dev->power_on ? ui_str(UI_STR_ON) : ui_str(UI_STR_OFF));
        lv_obj_set_style_text_color(s_status_label,
                                    dev->power_on ? lv_color_hex(0x2159C4) : lv_color_hex(0x4A5568), 0);
    }
}

static void ui_switch_detail_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    extern lv_obj_t *ui_Screen_Home;
    if (ui_Screen_Home) {
        lv_scr_load(ui_Screen_Home);
        ui_Screen_Home_update_device_card(s_switch_device);
    }
}

static void ui_switch_detail_settings_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_Screen_Device_Setting_show(s_switch_device);
}

static void ui_switch_detail_power_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!s_switch_device) {
        return;
    }
    rm_device_item_t *dev = (rm_device_item_t *)s_switch_device;
    if (!dev->online) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_DEVICE_OFFLINE));
        return;
    }
    bool power_on = !dev->power_on;
    const char *err = NULL;
    if (rainmaker_standard_switch_set_power(dev->node_id, power_on, &err) != ESP_OK) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), err ? err : ui_str(UI_STR_FAILED_SET_POWER));
        return;
    }
    dev->power_on = power_on;
    ui_switch_detail_refresh_from_device();
}

static void ui_switch_detail_screen_loaded_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SCREEN_LOADED) {
        return;
    }
    ui_switch_detail_refresh_from_device();
}

static lv_obj_t *ui_switch_detail_create_header(lv_obj_t *parent)
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
    lv_obj_add_event_cb(back, ui_switch_detail_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_arrow = lv_label_create(back);
    lv_label_set_text(back_arrow, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_arrow, lv_color_hex(0x2159C4), 0);
    lv_obj_center(back_arrow);

    s_title_label = lv_label_create(bar);
    lv_label_set_text(s_title_label, ui_str(UI_STR_SWITCH));
    lv_obj_set_style_text_font(s_title_label, ui_i18n_uses_cjk() ? ui_font_cjk_always() : ui_font_title(), 0);
    lv_obj_align(s_title_label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *settings = lv_btn_create(bar);
    lv_obj_clear_flag(settings, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(settings, ui_display_scale_px(40), ui_display_scale_px(40));
    lv_obj_align(settings, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(settings, LV_OPA_0, 0);
    lv_obj_set_style_border_width(settings, 0, 0);
    lv_obj_add_event_cb(settings, ui_switch_detail_settings_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *gear = lv_label_create(settings);
    lv_label_set_text(gear, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(gear, lv_color_hex(0x2159C4), 0);
    lv_obj_center(gear);

    return bar;
}

static void ui_switch_detail_create_content(lv_obj_t *parent)
{
    const int32_t pw = ui_display_scale_px(88);
    const int32_t pr = pw / 2;
    /* Centered power button with subtle shadow */
    s_power_btn = lv_btn_create(parent);
    lv_obj_clear_flag(s_power_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_power_btn, pw, pw);
    lv_obj_align(s_power_btn, LV_ALIGN_CENTER, 0, ui_display_scale_px(12));
    lv_obj_set_style_radius(s_power_btn, pr, 0);
    lv_obj_set_style_bg_color(s_power_btn, lv_color_hex(0x2159C4), 0);
    lv_obj_set_style_border_width(s_power_btn, 0, 0);
    lv_obj_set_style_shadow_width(s_power_btn, ui_display_scale_px(14), 0);
    lv_obj_set_style_shadow_color(s_power_btn, lv_color_hex(0x2159C4), 0);
    lv_obj_set_style_shadow_opa(s_power_btn, LV_OPA_20, 0);
    lv_obj_add_event_cb(s_power_btn, ui_switch_detail_power_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *power_icon = lv_label_create(s_power_btn);
    lv_label_set_text(power_icon, LV_SYMBOL_POWER);
    lv_obj_set_style_text_color(power_icon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(power_icon, &lv_font_montserrat_24, 0);
    lv_obj_center(power_icon);

    /* ON / OFF label below button */
    s_status_label = lv_label_create(parent);
    lv_label_set_text(s_status_label, ui_str(UI_STR_OFF));
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x4A5568), 0);
    lv_obj_set_style_text_font(s_status_label, ui_font_body(), 0);
    lv_obj_align(s_status_label, LV_ALIGN_CENTER, 0, ui_display_scale_px(12) + pr + ui_display_scale_px(20));
}

void ui_Screen_SwitchDetails_show(rm_device_item_handle_t device)
{
    if (device == NULL || ui_Screen_SwitchDetails == NULL) {
        return;
    }
    s_switch_device = device;
    ui_switch_detail_refresh_from_device();
    lv_scr_load(ui_Screen_SwitchDetails);
}

void ui_Screen_SwitchDetails_screen_init(void)
{
    ui_Screen_SwitchDetails = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_Screen_SwitchDetails, lv_color_hex(0xF5F8FF), 0);

    (void)ui_switch_detail_create_header(ui_Screen_SwitchDetails);
    ui_switch_detail_create_content(ui_Screen_SwitchDetails);

    lv_obj_add_event_cb(ui_Screen_SwitchDetails, ui_switch_detail_screen_loaded_cb, LV_EVENT_SCREEN_LOADED, NULL);
}

void ui_Screen_SwitchDetails_screen_destroy(void)
{
    if (ui_Screen_SwitchDetails) {
        lv_obj_del(ui_Screen_SwitchDetails);
    }
    ui_Screen_SwitchDetails = NULL;
    s_switch_device = NULL;
    s_title_label = NULL;
    s_power_btn = NULL;
    s_status_label = NULL;
}

void ui_Screen_SwitchDetails_apply_language(void)
{
    if (s_title_label) {
        lv_obj_set_style_text_font(s_title_label, ui_i18n_uses_cjk() ? ui_font_cjk_always() : ui_font_title(), 0);
    }
    if (s_status_label) {
        lv_obj_set_style_text_font(s_status_label, ui_font_body(), 0);
    }
    ui_switch_detail_refresh_from_device();
}
