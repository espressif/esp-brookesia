/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Light device detail screen: power, brightness, hue, saturation.
 */

#include <stdio.h>
#include <string.h>

#include "../ui.h"
#include "rainmaker_app_backend.h"
#include "rainmaker_device_group_manager.h"
#include "rainmaker_standard_device.h"

lv_obj_t *ui_Screen_LightDetail = NULL;

static rm_device_item_handle_t s_light_device = NULL;
/** Stable key for rebind after backend rebuilds the device list (avoids UAF on s_light_device). */
#define UI_LIGHT_DETAIL_NODE_ID_MAX 96
static char s_light_device_node_id[UI_LIGHT_DETAIL_NODE_ID_MAX];

static void ui_light_detail_store_node_id(rm_device_item_handle_t device)
{
    s_light_device_node_id[0] = '\0';
    if (device && device->node_id) {
        strncpy(s_light_device_node_id, device->node_id, sizeof(s_light_device_node_id) - 1);
        s_light_device_node_id[sizeof(s_light_device_node_id) - 1] = '\0';
    }
}

static void ui_light_detail_refresh_device_handle(void)
{
    if (s_light_device_node_id[0] == '\0') {
        return;
    }
    rm_device_item_handle_t h = rm_app_backend_get_device_handle_by_node_id(s_light_device_node_id);
    s_light_device = h;
}
static lv_obj_t *s_title_label = NULL;
static lv_obj_t *s_power_btn = NULL;
static lv_obj_t *s_brightness_slider = NULL;
static lv_obj_t *s_brightness_value_label = NULL;
static lv_obj_t *s_hue_slider = NULL;
static lv_obj_t *s_hue_value_label = NULL;
static lv_obj_t *s_saturation_slider = NULL;
static lv_obj_t *s_saturation_value_label = NULL;
static lv_obj_t *s_tab_white_label = NULL;
static lv_obj_t *s_tab_color_label = NULL;
static lv_obj_t *s_brightness_caption_lab = NULL;
static lv_obj_t *s_hue_caption_lab = NULL;
static lv_obj_t *s_saturation_caption_lab = NULL;
static lv_obj_t *s_brightness_section = NULL;
static lv_obj_t *s_hue_section = NULL;
static lv_obj_t *s_saturation_section = NULL;
static lv_obj_t *s_hue_rainbow_bar = NULL;
static lv_grad_dsc_t s_hue_rainbow_grad;
static bool s_hue_rainbow_grad_inited = false;
static lv_obj_t *s_saturation_bar = NULL;
static lv_grad_dsc_t s_saturation_grad;
static bool s_light_mode_color = true;

/* Slider UX: only draggable when pressing the knob (circle). */
#define UI_LIGHT_DETAIL_SLIDER_TRACK_H        6
#define UI_LIGHT_DETAIL_SLIDER_EXT_CLICK_PAD  32
/* NOTE: In LVGL, knob hit/size is largely driven by the slider object's height + knob padding. */
#define UI_LIGHT_DETAIL_SLIDER_OBJ_H          12
#define UI_LIGHT_DETAIL_SLIDER_KNOB_D         30
#define UI_LIGHT_DETAIL_SLIDER_KNOB_PAD       ((UI_LIGHT_DETAIL_SLIDER_KNOB_D - UI_LIGHT_DETAIL_SLIDER_OBJ_H) / 2)
#define UI_LIGHT_DETAIL_LABEL_ROW_H           18
#define UI_LIGHT_DETAIL_SLIDER_ROW_H          50
#define UI_LIGHT_DETAIL_SECTION_GAP           8
/* Hue track: horizontal multi-stop gradient (needs CONFIG_LV_GRADIENT_MAX_STOPS / LV_GRADIENT_MAX_STOPS >= 7). */
#define UI_LIGHT_DETAIL_HUE_RAINBOW_STOPS     7
/* Saturation track: S 0% -> 100% at current H, V=100 (needs LV_GRADIENT_MAX_STOPS >= 2). */
#define UI_LIGHT_DETAIL_SATURATION_STOPS      2

static const lv_color_t s_hue_rainbow_grad_colors[UI_LIGHT_DETAIL_HUE_RAINBOW_STOPS] = {
    LV_COLOR_MAKE(0xFF, 0x00, 0x00), /* R */
    LV_COLOR_MAKE(0xFF, 0xFF, 0x00), /* Y */
    LV_COLOR_MAKE(0x00, 0xFF, 0x00), /* G */
    LV_COLOR_MAKE(0x00, 0xFF, 0xFF), /* C */
    LV_COLOR_MAKE(0x00, 0x00, 0xFF), /* B */
    LV_COLOR_MAKE(0xFF, 0x00, 0xFF), /* M */
    LV_COLOR_MAKE(0xFF, 0x00, 0x00), /* R (wrap) */
};

static void ui_light_detail_hue_rainbow_ensure_grad(void)
{
    if (s_hue_rainbow_grad_inited) {
        return;
    }
    /* LVGL 9.2.x has lv_grad_dsc_t but no lv_grad_init_stops (added in 9.3). Inline the same logic. */
    const int num_stops = UI_LIGHT_DETAIL_HUE_RAINBOW_STOPS;
    s_hue_rainbow_grad.stops_count = (uint8_t)num_stops;
    for (int i = 0; i < num_stops; i++) {
        s_hue_rainbow_grad.stops[i].color = s_hue_rainbow_grad_colors[i];
        s_hue_rainbow_grad.stops[i].opa = LV_OPA_COVER;
        s_hue_rainbow_grad.stops[i].frac = (uint8_t)(255 * i / (num_stops - 1));
    }
    s_hue_rainbow_grad.dir = LV_GRAD_DIR_HOR;
    s_hue_rainbow_grad_inited = true;
}

static void ui_light_detail_hue_rainbow_apply_state(bool online)
{
    if (!s_hue_rainbow_bar) {
        return;
    }
    if (online) {
        ui_light_detail_hue_rainbow_ensure_grad();
        lv_obj_set_style_bg_grad(s_hue_rainbow_bar, &s_hue_rainbow_grad, 0);
        lv_obj_set_style_bg_opa(s_hue_rainbow_bar, LV_OPA_COVER, 0);
    } else {
        lv_obj_set_style_bg_grad(s_hue_rainbow_bar, NULL, 0);
        lv_obj_set_style_bg_color(s_hue_rainbow_bar, lv_color_hex(0x9CA3AF), 0);
        lv_obj_set_style_bg_opa(s_hue_rainbow_bar, LV_OPA_COVER, 0);
    }
}

static void ui_light_detail_saturation_grad_update_for_hue(int hue_deg)
{
    int h_norm = hue_deg % 360;
    if (h_norm < 0) {
        h_norm += 360;
    }
    uint16_t h = (uint16_t)h_norm;
    s_saturation_grad.stops_count = UI_LIGHT_DETAIL_SATURATION_STOPS;
    s_saturation_grad.stops[0].color = lv_color_hsv_to_rgb(h, 0, 100);
    s_saturation_grad.stops[0].opa = LV_OPA_COVER;
    s_saturation_grad.stops[0].frac = 0;
    s_saturation_grad.stops[1].color = lv_color_hsv_to_rgb(h, 100, 100);
    s_saturation_grad.stops[1].opa = LV_OPA_COVER;
    s_saturation_grad.stops[1].frac = 255;
    s_saturation_grad.dir = LV_GRAD_DIR_HOR;
}

static void ui_light_detail_saturation_bar_apply_state(bool online)
{
    if (!s_saturation_bar) {
        return;
    }
    if (online) {
        int hue = 0;
        if (s_hue_slider) {
            hue = lv_slider_get_value(s_hue_slider);
        } else if (s_light_device) {
            hue = s_light_device->light.hue;
        }
        ui_light_detail_saturation_grad_update_for_hue(hue);
        lv_obj_set_style_bg_grad(s_saturation_bar, &s_saturation_grad, 0);
        lv_obj_set_style_bg_opa(s_saturation_bar, LV_OPA_COVER, 0);
    } else {
        lv_obj_set_style_bg_grad(s_saturation_bar, NULL, 0);
        lv_obj_set_style_bg_color(s_saturation_bar, lv_color_hex(0x9CA3AF), 0);
        lv_obj_set_style_bg_opa(s_saturation_bar, LV_OPA_COVER, 0);
    }
}

static void ui_light_detail_style_slider_knob_only(lv_obj_t *slider)
{
    if (!slider) {
        return;
    }

    /* Only react when the knob area is hit (LV_EVENT_HIT_TEST in lv_slider). */
    lv_obj_add_flag(slider, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_add_flag(slider, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_ext_click_area(slider, UI_LIGHT_DETAIL_SLIDER_EXT_CLICK_PAD);
    /* Keep slider background transparent so padded ends are invisible */
    lv_obj_set_style_bg_opa(slider, LV_OPA_0, 0);

    /* Blue thin bar (track + fill). */
    lv_obj_set_style_height(slider, UI_LIGHT_DETAIL_SLIDER_TRACK_H, LV_PART_MAIN);
    lv_obj_set_style_height(slider, UI_LIGHT_DETAIL_SLIDER_TRACK_H, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, UI_LIGHT_DETAIL_SLIDER_TRACK_H / 2, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, UI_LIGHT_DETAIL_SLIDER_TRACK_H / 2, LV_PART_INDICATOR);
    /* Keep knob fully visible at both ends by insetting the track. */
    const int32_t knob_half = UI_LIGHT_DETAIL_SLIDER_KNOB_D / 2;
    lv_obj_set_style_pad_left(slider, knob_half, LV_PART_MAIN);
    lv_obj_set_style_pad_right(slider, knob_half, LV_PART_MAIN);
    lv_obj_set_style_pad_left(slider, knob_half, LV_PART_INDICATOR);
    lv_obj_set_style_pad_right(slider, knob_half, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xE2E8F0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x2E6BE6), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_90, LV_PART_INDICATOR);

    /* Knob (circle) look. */
    lv_obj_set_style_width(slider, UI_LIGHT_DETAIL_SLIDER_KNOB_D, LV_PART_KNOB);
    lv_obj_set_style_height(slider, UI_LIGHT_DETAIL_SLIDER_KNOB_D, LV_PART_KNOB);
    /* Increase knob's effective diameter in LVGL's internal calculations */
    lv_obj_set_style_pad_all(slider, UI_LIGHT_DETAIL_SLIDER_KNOB_PAD, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_border_width(slider, 2, LV_PART_KNOB);
    lv_obj_set_style_border_color(slider, lv_color_hex(0xD0D7E2), LV_PART_KNOB);
    lv_obj_set_style_shadow_width(slider, 6, LV_PART_KNOB);
    lv_obj_set_style_shadow_color(slider, lv_color_hex(0x000000), LV_PART_KNOB);
    lv_obj_set_style_shadow_opa(slider, LV_OPA_10, LV_PART_KNOB);
}

/* Hue: same geometry/hit-test as knob_only, but track+fill transparent so a rainbow bar shows through. */
static void ui_light_detail_style_hue_slider_knob_only(lv_obj_t *slider)
{
    if (!slider) {
        return;
    }

    lv_obj_add_flag(slider, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_add_flag(slider, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_ext_click_area(slider, UI_LIGHT_DETAIL_SLIDER_EXT_CLICK_PAD);
    lv_obj_set_style_bg_opa(slider, LV_OPA_0, 0);

    lv_obj_set_style_height(slider, UI_LIGHT_DETAIL_SLIDER_TRACK_H, LV_PART_MAIN);
    lv_obj_set_style_height(slider, UI_LIGHT_DETAIL_SLIDER_TRACK_H, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, UI_LIGHT_DETAIL_SLIDER_TRACK_H / 2, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, UI_LIGHT_DETAIL_SLIDER_TRACK_H / 2, LV_PART_INDICATOR);
    const int32_t knob_half = UI_LIGHT_DETAIL_SLIDER_KNOB_D / 2;
    lv_obj_set_style_pad_left(slider, knob_half, LV_PART_MAIN);
    lv_obj_set_style_pad_right(slider, knob_half, LV_PART_MAIN);
    lv_obj_set_style_pad_left(slider, knob_half, LV_PART_INDICATOR);
    lv_obj_set_style_pad_right(slider, knob_half, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_0, LV_PART_INDICATOR);

    lv_obj_set_style_width(slider, UI_LIGHT_DETAIL_SLIDER_KNOB_D, LV_PART_KNOB);
    lv_obj_set_style_height(slider, UI_LIGHT_DETAIL_SLIDER_KNOB_D, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, UI_LIGHT_DETAIL_SLIDER_KNOB_PAD, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_border_width(slider, 2, LV_PART_KNOB);
    lv_obj_set_style_border_color(slider, lv_color_hex(0xD0D7E2), LV_PART_KNOB);
    lv_obj_set_style_shadow_width(slider, 6, LV_PART_KNOB);
    lv_obj_set_style_shadow_color(slider, lv_color_hex(0x000000), LV_PART_KNOB);
    lv_obj_set_style_shadow_opa(slider, LV_OPA_10, LV_PART_KNOB);
}

static void ui_light_detail_refresh_from_device(void)
{
    ui_light_detail_refresh_device_handle();
    if (!s_light_device) {
        return;
    }
    if (s_title_label) {
        lv_label_set_text(s_title_label, s_light_device->name ? s_light_device->name : ui_str(UI_STR_LIGHT));
        lv_obj_set_style_text_font(s_title_label, ui_i18n_uses_cjk() ? ui_font_cjk_always() : ui_font_title(), 0);
    }
    if (s_power_btn) {
        if (s_light_device->power_on) {
            lv_obj_set_style_bg_color(s_power_btn, lv_color_hex(0x2E6BE6), 0);
        } else {
            lv_obj_set_style_bg_color(s_power_btn, lv_color_hex(0xA0B0C8), 0);
        }
        if (s_light_device->online) {
            lv_obj_add_flag(s_power_btn, LV_OBJ_FLAG_CLICKABLE);
        } else {
            lv_obj_clear_flag(s_power_btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(s_power_btn, lv_color_hex(0x9CA3AF), 0);
        }
    }
    if (s_brightness_slider) {
        lv_slider_set_value(s_brightness_slider, s_light_device->light.brightness, LV_ANIM_OFF);
    }
    if (s_brightness_value_label) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", s_light_device->light.brightness);
        lv_label_set_text(s_brightness_value_label, buf);
    }
    if (s_hue_slider) {
        lv_slider_set_value(s_hue_slider, s_light_device->light.hue, LV_ANIM_OFF);
    }
    if (s_hue_value_label) {
        char buf[24];
        ui_format_hue_value_string(s_light_device->light.hue, buf, sizeof(buf));
        lv_label_set_text(s_hue_value_label, buf);
    }
    if (s_saturation_slider) {
        lv_slider_set_value(s_saturation_slider, s_light_device->light.saturation, LV_ANIM_OFF);
    }
    if (s_saturation_value_label) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", s_light_device->light.saturation);
        lv_label_set_text(s_saturation_value_label, buf);
    }

    /* When offline: disable sliders (no drag); when online: enable */
    if (s_light_device->online) {
        if (s_brightness_slider) {
            lv_obj_add_flag(s_brightness_slider, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(s_brightness_slider, lv_color_hex(0xE2E8F0), LV_PART_MAIN);
            lv_obj_set_style_bg_color(s_brightness_slider, lv_color_hex(0x2E6BE6), LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(s_brightness_slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
        }
        if (s_hue_slider) {
            lv_obj_add_flag(s_hue_slider, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_opa(s_hue_slider, LV_OPA_0, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(s_hue_slider, LV_OPA_0, LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(s_hue_slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
            ui_light_detail_hue_rainbow_apply_state(true);
        }
        if (s_saturation_slider) {
            lv_obj_add_flag(s_saturation_slider, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_opa(s_saturation_slider, LV_OPA_0, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(s_saturation_slider, LV_OPA_0, LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(s_saturation_slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
            ui_light_detail_saturation_bar_apply_state(true);
        }
    } else {
        if (s_brightness_slider) {
            lv_obj_clear_flag(s_brightness_slider, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(s_brightness_slider, lv_color_hex(0x9CA3AF), LV_PART_MAIN);
            lv_obj_set_style_bg_color(s_brightness_slider, lv_color_hex(0x9CA3AF), LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(s_brightness_slider, lv_color_hex(0xC0C4CC), LV_PART_KNOB);
        }
        if (s_hue_slider) {
            lv_obj_clear_flag(s_hue_slider, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_opa(s_hue_slider, LV_OPA_0, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(s_hue_slider, LV_OPA_0, LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(s_hue_slider, lv_color_hex(0xC0C4CC), LV_PART_KNOB);
            ui_light_detail_hue_rainbow_apply_state(false);
        }
        if (s_saturation_slider) {
            lv_obj_clear_flag(s_saturation_slider, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_opa(s_saturation_slider, LV_OPA_0, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(s_saturation_slider, LV_OPA_0, LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(s_saturation_slider, lv_color_hex(0xC0C4CC), LV_PART_KNOB);
            ui_light_detail_saturation_bar_apply_state(false);
        }
    }
}

static void ui_light_detail_update_mode(bool color_mode)
{
    s_light_mode_color = color_mode;
    if (s_hue_section) {
        if (color_mode) {
            lv_obj_clear_flag(s_hue_section, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_hue_section, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (s_saturation_section) {
        if (color_mode) {
            lv_obj_clear_flag(s_saturation_section, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_saturation_section, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (s_tab_white_label) {
        lv_obj_set_style_text_color(
            s_tab_white_label,
            color_mode ? lv_color_hex(0x8AA0C8) : lv_color_hex(0x2E6BE6),
            0
        );
    }
    if (s_tab_color_label) {
        lv_obj_set_style_text_color(
            s_tab_color_label,
            color_mode ? lv_color_hex(0x2E6BE6) : lv_color_hex(0x8AA0C8),
            0
        );
    }
}

static void ui_light_detail_tab_white_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_light_detail_update_mode(false);
}

static void ui_light_detail_tab_color_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_light_detail_update_mode(true);
}

static void ui_light_detail_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_light_detail_refresh_device_handle();
    extern lv_obj_t *ui_Screen_Home;
    if (ui_Screen_Home) {
        lv_scr_load(ui_Screen_Home);
        /* Refresh only current device card */
        ui_Screen_Home_update_device_card(s_light_device);
    }
}

static void ui_light_detail_settings_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_light_detail_refresh_device_handle();
    ui_Screen_Device_Setting_show(s_light_device);
}

static void ui_light_detail_power_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_light_detail_refresh_device_handle();
    if (!s_light_device) {
        return;
    }

    if (!s_light_device->online) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_DEVICE_OFFLINE));
        goto exit;
    }

    bool power_on = !s_light_device->power_on;
    const char *err = NULL;
    if (rainmaker_standard_light_set_power(s_light_device->node_id, power_on, &err) != ESP_OK) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), err ? err : ui_str(UI_STR_FAILED_SET_POWER));
        goto exit;
    }
    s_light_device->power_on = power_on;
exit:
    ui_light_detail_refresh_from_device();
}

static void ui_light_detail_slider_cb(lv_event_t *e, int which)
{
    lv_event_code_t code = lv_event_get_code(e);
    ui_light_detail_refresh_device_handle();
    if (!s_light_device) {
        return;
    }

    lv_obj_t *slider = lv_event_get_target(e);
    int val = lv_slider_get_value(slider);

    /* During drag: update UI only, avoid cloud writes */
    if (code == LV_EVENT_VALUE_CHANGED) {
        char buf[24];
        if (which == 0 && s_brightness_value_label) {
            snprintf(buf, sizeof(buf), "%d%%", val);
            lv_label_set_text(s_brightness_value_label, buf);
        } else if (which == 1 && s_hue_value_label) {
            ui_format_hue_value_string(val, buf, sizeof(buf));
            lv_label_set_text(s_hue_value_label, buf);
            /* Saturation strip shows gradient for current hue — update while dragging hue */
            if (s_light_device && s_light_device->online && s_saturation_bar) {
                ui_light_detail_saturation_grad_update_for_hue(val);
                lv_obj_set_style_bg_grad(s_saturation_bar, &s_saturation_grad, 0);
                lv_obj_set_style_bg_opa(s_saturation_bar, LV_OPA_COVER, 0);
            }
        } else if (which == 2 && s_saturation_value_label) {
            snprintf(buf, sizeof(buf), "%d%%", val);
            lv_label_set_text(s_saturation_value_label, buf);
        }
        return;
    }

    /* Only commit to cloud on release */
    if (code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) {
        return;
    }

    if (!s_light_device->online) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_DEVICE_OFFLINE));
        goto exit;
    }

    int b = s_light_device->light.brightness;
    int h = s_light_device->light.hue;
    int s = s_light_device->light.saturation;
    if (which == 0) {
        b = val;
        if (s_brightness_value_label) {
            const char *err = NULL;
            if (rainmaker_standard_light_set_brightness(s_light_device->node_id, b, &err) != ESP_OK) {
                ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), err ? err : ui_str(UI_STR_FAILED_SET_BRIGHTNESS));
                goto exit;
            }
            char buf[16];
            snprintf(buf, sizeof(buf), "%d%%", b);
            lv_label_set_text(s_brightness_value_label, buf);
            s_light_device->light.brightness = b;
        }
    } else if (which == 1) {
        h = val;
        if (s_hue_value_label) {
            const char *err = NULL;
            if (rainmaker_standard_light_set_hue(s_light_device->node_id, h, &err) != ESP_OK) {
                ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), err ? err : ui_str(UI_STR_FAILED_SET_HUE));
                goto exit;
            }
            char buf[24];
            ui_format_hue_value_string(h, buf, sizeof(buf));
            lv_label_set_text(s_hue_value_label, buf);
            s_light_device->light.hue = h;
        }
    } else {
        s = val;
        if (s_saturation_value_label) {
            const char *err = NULL;
            if (rainmaker_standard_light_set_saturation(s_light_device->node_id, s, &err) != ESP_OK) {
                ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), err ? err : ui_str(UI_STR_FAILED_SET_SAT));
                goto exit;
            }
            char buf[16];
            snprintf(buf, sizeof(buf), "%d%%", s);
            lv_label_set_text(s_saturation_value_label, buf);
            s_light_device->light.saturation = s;
        }
    }
exit:
    ui_light_detail_refresh_from_device();
}

static void ui_light_detail_brightness_cb(lv_event_t *e)
{
    ui_light_detail_slider_cb(e, 0);
}

static void ui_light_detail_hue_cb(lv_event_t *e)
{
    ui_light_detail_slider_cb(e, 1);
}

static void ui_light_detail_saturation_cb(lv_event_t *e)
{
    ui_light_detail_slider_cb(e, 2);
}

static void ui_light_detail_screen_loaded_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SCREEN_LOADED) {
        return;
    }
    ui_light_detail_refresh_from_device();
    /* Default to Color mode when entering from card */
    ui_light_detail_update_mode(true);
}

static lv_obj_t *ui_light_detail_create_header(lv_obj_t *parent)
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
    lv_obj_add_event_cb(back, ui_light_detail_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_arrow = lv_label_create(back);
    lv_label_set_text(back_arrow, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_arrow, lv_color_hex(0x2E6BE6), 0);
    lv_obj_center(back_arrow);

    s_title_label = lv_label_create(bar);
    lv_label_set_text(s_title_label, ui_str(UI_STR_LIGHT));
    lv_obj_set_style_text_font(s_title_label, ui_i18n_uses_cjk() ? ui_font_cjk_always() : ui_font_title(), 0);
    lv_obj_align(s_title_label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *settings = lv_btn_create(bar);
    lv_obj_clear_flag(settings, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(settings, ui_display_scale_px(40), ui_display_scale_px(40));
    lv_obj_align(settings, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(settings, LV_OPA_0, 0);
    lv_obj_set_style_border_width(settings, 0, 0);
    lv_obj_add_event_cb(settings, ui_light_detail_settings_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *gear = lv_label_create(settings);
    lv_label_set_text(gear, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(gear, lv_color_hex(0x2E6BE6), 0);
    lv_obj_center(gear);

    return bar;
}

static void ui_light_detail_create_content(lv_obj_t *parent)
{
    int32_t y = ui_display_scale_px(40);
    const int32_t tab_h = ui_display_scale_px(32);

    /* Tabs: White / Color */
    lv_obj_t *tabs = lv_obj_create(parent);
    lv_obj_clear_flag(tabs, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(tabs, ui_display_scale_px(160), tab_h);
    lv_obj_align(tabs, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_bg_opa(tabs, LV_OPA_0, 0);
    lv_obj_set_style_border_width(tabs, 0, 0);
    s_tab_white_label = lv_label_create(tabs);
    lv_label_set_text(s_tab_white_label, ui_str(UI_STR_WHITE));
    lv_obj_set_style_text_color(s_tab_white_label, lv_color_hex(0x8AA0C8), 0);
    lv_obj_align(s_tab_white_label, LV_ALIGN_LEFT_MID, ui_display_scale_px(8), 0);
    lv_obj_add_flag(s_tab_white_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_tab_white_label, ui_light_detail_tab_white_cb, LV_EVENT_CLICKED, NULL);

    s_tab_color_label = lv_label_create(tabs);
    lv_label_set_text(s_tab_color_label, ui_str(UI_STR_COLOR));
    lv_obj_set_style_text_color(s_tab_color_label, lv_color_hex(0x2E6BE6), 0);
    lv_obj_align(s_tab_color_label, LV_ALIGN_RIGHT_MID, -ui_display_scale_px(8), 0);
    lv_obj_add_flag(s_tab_color_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_tab_color_label, ui_light_detail_tab_color_cb, LV_EVENT_CLICKED, NULL);
    y += tab_h;

    /* Power button */
    const int32_t pwr = ui_display_scale_px(72);
    s_power_btn = lv_btn_create(parent);
    lv_obj_clear_flag(s_power_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_power_btn, pwr, pwr);
    lv_obj_align(s_power_btn, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_radius(s_power_btn, pwr / 2, 0);
    lv_obj_set_style_bg_color(s_power_btn, lv_color_hex(0x2E6BE6), 0);
    lv_obj_set_style_border_width(s_power_btn, 0, 0);
    lv_obj_add_event_cb(s_power_btn, ui_light_detail_power_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *power_icon = lv_label_create(s_power_btn);
    lv_label_set_text(power_icon, LV_SYMBOL_POWER);
    lv_obj_set_style_text_color(power_icon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(power_icon, &lv_font_montserrat_20, 0);
    lv_obj_center(power_icon);
    y += pwr;

    /* Brightness */
    const int32_t b_slider_row_h = UI_LIGHT_DETAIL_SLIDER_KNOB_D + 8;
    const int32_t b_section_h = UI_LIGHT_DETAIL_LABEL_ROW_H + b_slider_row_h;
    s_brightness_section = lv_obj_create(parent);
    lv_obj_clear_flag(s_brightness_section, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_brightness_section, lv_pct(90), b_section_h);
    lv_obj_align(s_brightness_section, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_bg_opa(s_brightness_section, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_brightness_section, 0, 0);
    lv_obj_add_flag(s_brightness_section, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_obj_t *b_label_row = lv_obj_create(s_brightness_section);
    lv_obj_clear_flag(b_label_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(b_label_row, lv_pct(100), UI_LIGHT_DETAIL_LABEL_ROW_H);
    lv_obj_align(b_label_row, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(b_label_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(b_label_row, 0, 0);
    lv_obj_t *b_lab = lv_label_create(b_label_row);
    s_brightness_caption_lab = b_lab;
    lv_label_set_text(b_lab, ui_str(UI_STR_BRIGHTNESS));
    lv_obj_set_style_text_color(b_lab, lv_color_hex(0xA0AEC0), 0);
    lv_obj_align(b_lab, LV_ALIGN_LEFT_MID, 0, 0);
    s_brightness_value_label = lv_label_create(b_label_row);
    lv_label_set_text(s_brightness_value_label, "0%");
    lv_obj_set_style_text_color(s_brightness_value_label, lv_color_hex(0xA0AEC0), 0);
    lv_obj_align(s_brightness_value_label, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_t *b_slider_row = lv_obj_create(s_brightness_section);
    lv_obj_clear_flag(b_slider_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(b_slider_row, lv_pct(100), b_slider_row_h);
    lv_obj_align(b_slider_row, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(b_slider_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(b_slider_row, 0, 0);
    lv_obj_add_flag(b_slider_row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    s_brightness_slider = lv_slider_create(b_slider_row);
    lv_obj_set_width(s_brightness_slider, lv_pct(100));
    lv_obj_set_height(s_brightness_slider, UI_LIGHT_DETAIL_SLIDER_KNOB_D);
    lv_obj_align(s_brightness_slider, LV_ALIGN_CENTER, 0, 0);
    lv_slider_set_range(s_brightness_slider, 0, 100);
    ui_light_detail_style_slider_knob_only(s_brightness_slider);
    lv_obj_add_event_cb(s_brightness_slider, ui_light_detail_brightness_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_brightness_slider, ui_light_detail_brightness_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(s_brightness_slider, ui_light_detail_brightness_cb, LV_EVENT_PRESS_LOST, NULL);
    y += b_section_h + UI_LIGHT_DETAIL_SECTION_GAP;

    /* Hue */
    const int32_t h_slider_row_h = UI_LIGHT_DETAIL_SLIDER_KNOB_D + 8;
    const int32_t h_section_h = UI_LIGHT_DETAIL_LABEL_ROW_H + h_slider_row_h;
    s_hue_section = lv_obj_create(parent);
    lv_obj_clear_flag(s_hue_section, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_hue_section, lv_pct(90), h_section_h);
    lv_obj_align(s_hue_section, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_bg_opa(s_hue_section, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_hue_section, 0, 0);
    lv_obj_add_flag(s_hue_section, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_obj_t *h_label_row = lv_obj_create(s_hue_section);
    lv_obj_clear_flag(h_label_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(h_label_row, lv_pct(100), UI_LIGHT_DETAIL_LABEL_ROW_H);
    lv_obj_align(h_label_row, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(h_label_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(h_label_row, 0, 0);
    lv_obj_t *h_lab = lv_label_create(h_label_row);
    s_hue_caption_lab = h_lab;
    lv_label_set_text(h_lab, ui_str(UI_STR_HUE));
    lv_obj_set_style_text_color(h_lab, lv_color_hex(0xA0AEC0), 0);
    lv_obj_align(h_lab, LV_ALIGN_LEFT_MID, 0, 0);
    s_hue_value_label = lv_label_create(h_label_row);
    {
        char hb[24];
        ui_format_hue_value_string(0, hb, sizeof(hb));
        lv_label_set_text(s_hue_value_label, hb);
    }
    /* Hue suffix is "°" (EN) or "度" (ZH); use Latin font for EN, CJK font for ZH */
    lv_obj_set_style_text_font(s_hue_value_label, ui_i18n_uses_cjk() ? ui_font_cjk_always() : ui_font_body(), 0);
    lv_obj_set_style_text_color(s_hue_value_label, lv_color_hex(0xA0AEC0), 0);
    lv_obj_align(s_hue_value_label, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_t *h_slider_row = lv_obj_create(s_hue_section);
    lv_obj_clear_flag(h_slider_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(h_slider_row, lv_pct(100), h_slider_row_h);
    lv_obj_align(h_slider_row, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(h_slider_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(h_slider_row, 0, 0);
    lv_obj_add_flag(h_slider_row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    /* Rainbow track behind slider (transparent MAIN/INDICATOR on hue slider). */
    {
        const int32_t knob_half = UI_LIGHT_DETAIL_SLIDER_KNOB_D / 2;
        s_hue_rainbow_bar = lv_obj_create(h_slider_row);
        lv_obj_remove_style_all(s_hue_rainbow_bar);
        lv_obj_clear_flag(s_hue_rainbow_bar, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_width(s_hue_rainbow_bar, lv_pct(100));
        lv_obj_set_height(s_hue_rainbow_bar, UI_LIGHT_DETAIL_SLIDER_TRACK_H);
        lv_obj_set_style_pad_left(s_hue_rainbow_bar, knob_half, 0);
        lv_obj_set_style_pad_right(s_hue_rainbow_bar, knob_half, 0);
        lv_obj_set_style_bg_opa(s_hue_rainbow_bar, LV_OPA_0, 0);
        lv_obj_set_style_border_width(s_hue_rainbow_bar, 0, 0);
        lv_obj_set_style_radius(s_hue_rainbow_bar, UI_LIGHT_DETAIL_SLIDER_TRACK_H / 2, 0);
        lv_obj_set_style_clip_corner(s_hue_rainbow_bar, true, 0);
        lv_obj_align(s_hue_rainbow_bar, LV_ALIGN_CENTER, 0, 0);
        ui_light_detail_hue_rainbow_apply_state(true);
    }

    s_hue_slider = lv_slider_create(h_slider_row);
    lv_obj_set_width(s_hue_slider, lv_pct(100));
    lv_obj_set_height(s_hue_slider, UI_LIGHT_DETAIL_SLIDER_KNOB_D);
    lv_obj_align(s_hue_slider, LV_ALIGN_CENTER, 0, 0);
    lv_slider_set_range(s_hue_slider, 0, 360);
    ui_light_detail_style_hue_slider_knob_only(s_hue_slider);
    lv_obj_add_event_cb(s_hue_slider, ui_light_detail_hue_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_hue_slider, ui_light_detail_hue_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(s_hue_slider, ui_light_detail_hue_cb, LV_EVENT_PRESS_LOST, NULL);
    y += h_section_h + UI_LIGHT_DETAIL_SECTION_GAP;

    /* Saturation */
    const int32_t s_slider_row_h = UI_LIGHT_DETAIL_SLIDER_KNOB_D + 8;
    const int32_t s_section_h = UI_LIGHT_DETAIL_LABEL_ROW_H + s_slider_row_h;
    s_saturation_section = lv_obj_create(parent);
    lv_obj_clear_flag(s_saturation_section, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_saturation_section, lv_pct(90), s_section_h);
    lv_obj_align(s_saturation_section, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_bg_opa(s_saturation_section, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_saturation_section, 0, 0);
    lv_obj_add_flag(s_saturation_section, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_obj_t *s_label_row = lv_obj_create(s_saturation_section);
    lv_obj_clear_flag(s_label_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_label_row, lv_pct(100), UI_LIGHT_DETAIL_LABEL_ROW_H);
    lv_obj_align(s_label_row, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(s_label_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_label_row, 0, 0);
    lv_obj_t *s_lab = lv_label_create(s_label_row);
    s_saturation_caption_lab = s_lab;
    lv_label_set_text(s_lab, ui_str(UI_STR_SATURATION));
    lv_obj_set_style_text_color(s_lab, lv_color_hex(0xA0AEC0), 0);
    lv_obj_align(s_lab, LV_ALIGN_LEFT_MID, 0, 0);
    s_saturation_value_label = lv_label_create(s_label_row);
    lv_label_set_text(s_saturation_value_label, "0%");
    lv_obj_set_style_text_color(s_saturation_value_label, lv_color_hex(0xA0AEC0), 0);
    lv_obj_align(s_saturation_value_label, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_t *s_slider_row = lv_obj_create(s_saturation_section);
    lv_obj_clear_flag(s_slider_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_slider_row, lv_pct(100), s_slider_row_h);
    lv_obj_align(s_slider_row, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(s_slider_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_slider_row, 0, 0);
    lv_obj_add_flag(s_slider_row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    /* Saturation track: low S -> full S at current hue (transparent MAIN/INDICATOR on slider). */
    {
        const int32_t knob_half = UI_LIGHT_DETAIL_SLIDER_KNOB_D / 2;
        s_saturation_bar = lv_obj_create(s_slider_row);
        lv_obj_remove_style_all(s_saturation_bar);
        lv_obj_clear_flag(s_saturation_bar, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_width(s_saturation_bar, lv_pct(100));
        lv_obj_set_height(s_saturation_bar, UI_LIGHT_DETAIL_SLIDER_TRACK_H);
        lv_obj_set_style_pad_left(s_saturation_bar, knob_half, 0);
        lv_obj_set_style_pad_right(s_saturation_bar, knob_half, 0);
        lv_obj_set_style_bg_opa(s_saturation_bar, LV_OPA_0, 0);
        lv_obj_set_style_border_width(s_saturation_bar, 0, 0);
        lv_obj_set_style_radius(s_saturation_bar, UI_LIGHT_DETAIL_SLIDER_TRACK_H / 2, 0);
        lv_obj_set_style_clip_corner(s_saturation_bar, true, 0);
        lv_obj_align(s_saturation_bar, LV_ALIGN_CENTER, 0, 0);
        ui_light_detail_saturation_bar_apply_state(true);
    }

    s_saturation_slider = lv_slider_create(s_slider_row);
    lv_obj_set_width(s_saturation_slider, lv_pct(100));
    lv_obj_set_height(s_saturation_slider, UI_LIGHT_DETAIL_SLIDER_KNOB_D);
    lv_obj_align(s_saturation_slider, LV_ALIGN_CENTER, 0, 0);
    lv_slider_set_range(s_saturation_slider, 0, 100);
    ui_light_detail_style_hue_slider_knob_only(s_saturation_slider);
    lv_obj_add_event_cb(s_saturation_slider, ui_light_detail_saturation_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_saturation_slider, ui_light_detail_saturation_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(s_saturation_slider, ui_light_detail_saturation_cb, LV_EVENT_PRESS_LOST, NULL);
    /* Only add blank line to make satuation not at the bottom of the screen */
    y += UI_LIGHT_DETAIL_SLIDER_ROW_H + UI_LIGHT_DETAIL_SECTION_GAP;
    lv_obj_t *blank_line = lv_obj_create(parent);
    lv_obj_clear_flag(blank_line, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(blank_line, lv_pct(100), 48);
    lv_obj_align(blank_line, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_bg_opa(blank_line, LV_OPA_0, 0);
    lv_obj_set_style_border_width(blank_line, 0, 0);
}

void ui_Screen_LightDetail_show(rm_device_item_handle_t device)
{
    if (device == NULL || ui_Screen_LightDetail == NULL) {
        return;
    }
    ui_light_detail_store_node_id(device);
    s_light_device = device;
    ui_light_detail_refresh_from_device();
    lv_scr_load(ui_Screen_LightDetail);
}

void ui_Screen_LightDetail_screen_init(void)
{
    ui_Screen_LightDetail = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_Screen_LightDetail, lv_color_hex(0xF5F8FF), 0);

    (void)ui_light_detail_create_header(ui_Screen_LightDetail);
    ui_light_detail_create_content(ui_Screen_LightDetail);

    lv_obj_add_event_cb(ui_Screen_LightDetail, ui_light_detail_screen_loaded_cb, LV_EVENT_SCREEN_LOADED, NULL);
}

void ui_Screen_LightDetail_screen_destroy(void)
{
    if (ui_Screen_LightDetail) {
        lv_obj_del(ui_Screen_LightDetail);
    }
    ui_Screen_LightDetail = NULL;
    s_light_device = NULL;
    s_light_device_node_id[0] = '\0';
    s_title_label = NULL;
    s_power_btn = NULL;
    s_brightness_slider = NULL;
    s_brightness_value_label = NULL;
    s_hue_slider = NULL;
    s_hue_value_label = NULL;
    s_saturation_slider = NULL;
    s_saturation_value_label = NULL;
    s_tab_white_label = NULL;
    s_tab_color_label = NULL;
    s_brightness_caption_lab = NULL;
    s_hue_caption_lab = NULL;
    s_saturation_caption_lab = NULL;
    s_brightness_section = NULL;
    s_hue_section = NULL;
    s_saturation_section = NULL;
    s_hue_rainbow_bar = NULL;
    s_hue_rainbow_grad_inited = false;
    s_saturation_bar = NULL;
}

void ui_Screen_LightDetail_apply_language(void)
{
    if (s_tab_white_label) {
        lv_label_set_text(s_tab_white_label, ui_str(UI_STR_WHITE));
        lv_obj_set_style_text_font(s_tab_white_label, ui_font_body(), 0);
    }
    if (s_tab_color_label) {
        lv_label_set_text(s_tab_color_label, ui_str(UI_STR_COLOR));
        lv_obj_set_style_text_font(s_tab_color_label, ui_font_body(), 0);
    }
    if (s_brightness_caption_lab) {
        lv_label_set_text(s_brightness_caption_lab, ui_str(UI_STR_BRIGHTNESS));
        lv_obj_set_style_text_font(s_brightness_caption_lab, ui_font_body(), 0);
    }
    if (s_hue_caption_lab) {
        lv_label_set_text(s_hue_caption_lab, ui_str(UI_STR_HUE));
        lv_obj_set_style_text_font(s_hue_caption_lab, ui_font_body(), 0);
    }
    if (s_hue_value_label) {
        lv_obj_set_style_text_font(s_hue_value_label, ui_i18n_uses_cjk() ? ui_font_cjk_always() : ui_font_body(), 0);
    }
    if (s_saturation_caption_lab) {
        lv_label_set_text(s_saturation_caption_lab, ui_str(UI_STR_SATURATION));
        lv_obj_set_style_text_font(s_saturation_caption_lab, ui_font_body(), 0);
    }
    if (s_title_label) {
        lv_obj_set_style_text_font(s_title_label, ui_i18n_uses_cjk() ? ui_font_cjk_always() : ui_font_title(), 0);
    }
    ui_light_detail_refresh_from_device();
}
