/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Select Actions: list of actions for a device (Power, Brightness, Hue, Saturation for Light; etc.).
 * Options are built per device type and whether_has_power.
 * Click Power -> bottom sheet with toggle; click Brightness -> bottom sheet with 0-100 slider.
 * List shows current setting on the right; Done returns and prints values.
 */

#include "../ui.h"
#include "rainmaker_app_backend.h"
#include "ui_Screen_SelectActions.h"
#include "ui_Screen_SelectDevices.h"
#include "ui_Screen_CreateSchedule.h"
#include "ui_Screen_CreateScene.h"
#include "rainmaker_device_group_manager.h"
#include "rainmaker_list_manager.h"
#include "rainmaker_app_backend_util.h"
#include "rainmaker_standard_device.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

lv_obj_t *ui_Screen_SelectActions = NULL;

typedef struct {
    bool power_on;
    bool power_set;
    union {
        rm_standard_light_device_t light;
        rm_standard_fan_device_t fan;
    } product;
    struct {
        bool brightness;
        bool hue;
        bool saturation;
        bool speed;
    } set;
} select_actions_state_t;

typedef struct {
    rm_device_item_handle_t handle;
    select_actions_state_t state;
    rm_list_item_t *edit_item;
    int edit_action_index;
} select_actions_ctx_t;

static select_actions_ctx_t s_select_actions_ctx = {NULL, {0}, NULL, -1};

static lv_obj_t *s_actions_list = NULL;
static lv_obj_t *s_select_actions_title_label = NULL;
static lv_obj_t *s_select_actions_done_lab = NULL;

/* Light hue/saturation sheets: match ui_Screen_Light_Detail (gradient track + knob-only transparent slider). */
#define SELECT_ACTIONS_LIGHT_SLIDER_TRACK_H       6
#define SELECT_ACTIONS_LIGHT_SLIDER_EXT_CLICK_PAD 32
#define SELECT_ACTIONS_LIGHT_SLIDER_OBJ_H           12
#define SELECT_ACTIONS_LIGHT_SLIDER_KNOB_D        30
#define SELECT_ACTIONS_LIGHT_SLIDER_KNOB_PAD \
    ((SELECT_ACTIONS_LIGHT_SLIDER_KNOB_D - SELECT_ACTIONS_LIGHT_SLIDER_OBJ_H) / 2)
#define SELECT_ACTIONS_LIGHT_HUE_RAINBOW_STOPS     7
#define SELECT_ACTIONS_LIGHT_SATURATION_STOPS      2

static lv_grad_dsc_t s_sa_hue_rainbow_grad;
static bool s_sa_hue_rainbow_grad_inited = false;
static lv_grad_dsc_t s_sa_saturation_grad;

static const lv_color_t s_sa_hue_rainbow_grad_colors[SELECT_ACTIONS_LIGHT_HUE_RAINBOW_STOPS] = {
    LV_COLOR_MAKE(0xFF, 0x00, 0x00),
    LV_COLOR_MAKE(0xFF, 0xFF, 0x00),
    LV_COLOR_MAKE(0x00, 0xFF, 0x00),
    LV_COLOR_MAKE(0x00, 0xFF, 0xFF),
    LV_COLOR_MAKE(0x00, 0x00, 0xFF),
    LV_COLOR_MAKE(0xFF, 0x00, 0xFF),
    LV_COLOR_MAKE(0xFF, 0x00, 0x00),
};

static void select_actions_hue_rainbow_ensure_grad(void)
{
    if (s_sa_hue_rainbow_grad_inited) {
        return;
    }
    const int num_stops = SELECT_ACTIONS_LIGHT_HUE_RAINBOW_STOPS;
    s_sa_hue_rainbow_grad.stops_count = (uint8_t)num_stops;
    for (int i = 0; i < num_stops; i++) {
        s_sa_hue_rainbow_grad.stops[i].color = s_sa_hue_rainbow_grad_colors[i];
        s_sa_hue_rainbow_grad.stops[i].opa = LV_OPA_COVER;
        s_sa_hue_rainbow_grad.stops[i].frac = (uint8_t)(255 * i / (num_stops - 1));
    }
    s_sa_hue_rainbow_grad.dir = LV_GRAD_DIR_HOR;
    s_sa_hue_rainbow_grad_inited = true;
}

static void select_actions_saturation_grad_update_for_hue(int hue_deg)
{
    int h_norm = hue_deg % 360;
    if (h_norm < 0) {
        h_norm += 360;
    }
    uint16_t h = (uint16_t)h_norm;
    s_sa_saturation_grad.stops_count = SELECT_ACTIONS_LIGHT_SATURATION_STOPS;
    s_sa_saturation_grad.stops[0].color = lv_color_hsv_to_rgb(h, 0, 100);
    s_sa_saturation_grad.stops[0].opa = LV_OPA_COVER;
    s_sa_saturation_grad.stops[0].frac = 0;
    s_sa_saturation_grad.stops[1].color = lv_color_hsv_to_rgb(h, 100, 100);
    s_sa_saturation_grad.stops[1].opa = LV_OPA_COVER;
    s_sa_saturation_grad.stops[1].frac = 255;
    s_sa_saturation_grad.dir = LV_GRAD_DIR_HOR;
}

static void select_actions_style_light_hue_slider_knob_only(lv_obj_t *slider)
{
    if (!slider) {
        return;
    }
    lv_obj_add_flag(slider, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_add_flag(slider, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_ext_click_area(slider, SELECT_ACTIONS_LIGHT_SLIDER_EXT_CLICK_PAD);
    lv_obj_set_style_bg_opa(slider, LV_OPA_0, 0);

    lv_obj_set_style_height(slider, SELECT_ACTIONS_LIGHT_SLIDER_TRACK_H, LV_PART_MAIN);
    lv_obj_set_style_height(slider, SELECT_ACTIONS_LIGHT_SLIDER_TRACK_H, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, SELECT_ACTIONS_LIGHT_SLIDER_TRACK_H / 2, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, SELECT_ACTIONS_LIGHT_SLIDER_TRACK_H / 2, LV_PART_INDICATOR);
    const int32_t knob_half = SELECT_ACTIONS_LIGHT_SLIDER_KNOB_D / 2;
    lv_obj_set_style_pad_left(slider, knob_half, LV_PART_MAIN);
    lv_obj_set_style_pad_right(slider, knob_half, LV_PART_MAIN);
    lv_obj_set_style_pad_left(slider, knob_half, LV_PART_INDICATOR);
    lv_obj_set_style_pad_right(slider, knob_half, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_0, LV_PART_INDICATOR);

    lv_obj_set_style_width(slider, SELECT_ACTIONS_LIGHT_SLIDER_KNOB_D, LV_PART_KNOB);
    lv_obj_set_style_height(slider, SELECT_ACTIONS_LIGHT_SLIDER_KNOB_D, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, SELECT_ACTIONS_LIGHT_SLIDER_KNOB_PAD, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_border_width(slider, 2, LV_PART_KNOB);
    lv_obj_set_style_border_color(slider, lv_color_hex(0xD0D7E2), LV_PART_KNOB);
    lv_obj_set_style_shadow_width(slider, 6, LV_PART_KNOB);
    lv_obj_set_style_shadow_color(slider, lv_color_hex(0x000000), LV_PART_KNOB);
    lv_obj_set_style_shadow_opa(slider, LV_OPA_10, LV_PART_KNOB);
}

static lv_obj_t *select_actions_slider_row_find_slider(lv_obj_t *slider_row)
{
    if (!slider_row) {
        return NULL;
    }
    uint32_t cnt = lv_obj_get_child_cnt(slider_row);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *ch = lv_obj_get_child(slider_row, i);
        if (ch && lv_obj_check_type(ch, &lv_slider_class)) {
            return ch;
        }
    }
    return NULL;
}

static const char *select_actions_action_display_name(const char *action)
{
    if (!action) {
        return "";
    }
    if (strcmp(action, RM_STANDARD_DEVICE_POWER) == 0) {
        return ui_str(UI_STR_POWER);
    }
    if (strcmp(action, RM_STANDARD_LIGHT_DEVICE_BRIGHTNESS) == 0) {
        return ui_str(UI_STR_BRIGHTNESS);
    }
    if (strcmp(action, RM_STANDARD_LIGHT_DEVICE_HUE) == 0) {
        return ui_str(UI_STR_HUE);
    }
    if (strcmp(action, RM_STANDARD_LIGHT_DEVICE_SATURATION) == 0) {
        return ui_str(UI_STR_SATURATION);
    }
    if (strcmp(action, RM_STANDARD_FAN_DEVICE_SPEED) == 0) {
        return ui_str(UI_STR_SPEED);
    }
    return action;
}

typedef struct {
    const char *action;
    lv_obj_t *value_label;
} row_ctx_t;

/* When options is NULL, only count is returned. Otherwise fill up to max and return count. */
typedef int (*get_options_fn_t)(rm_device_item_handle_t handle, const char **options, int max);

/** Light: Power (if has), Brightness, Hue, Saturation */
static int select_actions_get_light_options(rm_device_item_handle_t handle, const char **options, int max)
{
    int n = 0;
    if (handle->whether_has_power && (options == NULL || n < max)) {
        if (options) {
            options[n] = RM_STANDARD_DEVICE_POWER;
        }
        n++;
    }
    if (options == NULL || n < max) {
        if (options) {
            options[n] = RM_STANDARD_LIGHT_DEVICE_BRIGHTNESS;
        }
        n++;
    }
    if (options == NULL || n < max) {
        if (options) {
            options[n] = RM_STANDARD_LIGHT_DEVICE_HUE;
        }
        n++;
    }
    if (options == NULL || n < max) {
        if (options) {
            options[n] = RM_STANDARD_LIGHT_DEVICE_SATURATION;
        }
        n++;
    }
    return n;
}

/** Switch: Power (if has) */
static int select_actions_get_switch_options(rm_device_item_handle_t handle, const char **options, int max)
{
    int n = 0;
    if (handle->whether_has_power && (options == NULL || n < max)) {
        if (options) {
            options[n] = RM_STANDARD_DEVICE_POWER;
        }
        n++;
    }
    return n;
}

/** Fan: Power (if has), Speed */
static int select_actions_get_fan_options(rm_device_item_handle_t handle, const char **options, int max)
{
    int n = 0;
    if (handle->whether_has_power && (options == NULL || n < max)) {
        if (options) {
            options[n] = RM_STANDARD_DEVICE_POWER;
        }
        n++;
    }
    if (options == NULL || n < max) {
        if (options) {
            options[n] = RM_STANDARD_FAN_DEVICE_SPEED;
        }
        n++;
    }
    return n;
}

/** Build action options for the given device. *out_options (UI_LV_MALLOC) and *out_count set; caller must UI_LV_FREE(*out_options). */
static bool select_actions_build_options(rm_device_item_handle_t handle, const char ***out_options, int *out_count)
{

    if (!out_options || !out_count) {
        *out_options = NULL;
        *out_count = 0;
        return false;
    }
    get_options_fn_t fn = NULL;
    switch (handle->type) {
    case RAINMAKER_APP_DEVICE_TYPE_LIGHT:
        fn = select_actions_get_light_options;
        break;
    case RAINMAKER_APP_DEVICE_TYPE_SWITCH:
        fn = select_actions_get_switch_options;
        break;
    case RAINMAKER_APP_DEVICE_TYPE_FAN:
        fn = select_actions_get_fan_options;
        break;
    default:
        *out_options = NULL;
        *out_count = 0;
        return false;
    }
    int n = fn(handle, NULL, 0);
    if (n <= 0) {
        *out_options = NULL;
        *out_count = 0;
        return false;
    }
    const char **opts = (const char **)UI_LV_MALLOC((size_t)n * sizeof(const char *));
    if (!opts) {
        *out_options = NULL;
        *out_count = 0;
        return false;
    }
    fn(handle, opts, n);
    *out_options = opts;
    *out_count = n;
    return true;
}

static void select_actions_reset_state(void)
{
    memset(&s_select_actions_ctx.state, 0, sizeof(select_actions_state_t));
}

/** Parse action JSON (e.g. {"Switch":{"Power":true}}) and set s_power_on, s_brightness, etc. */
/** Parse action JSON according to device type: only read params supported by this device. */
static void select_actions_parse_action_json_to_state(rm_device_item_handle_t handle, const char *action_json)
{
    if (!handle || !action_json || action_json[0] == '\0') {
        return;
    }
    /* Reset state for this device so we don't carry over values from other device types */
    select_actions_reset_state();

    cJSON *root = cJSON_Parse(action_json);
    if (!root || !cJSON_IsObject(root)) {
        if (root) {
            cJSON_Delete(root);
        }
        return;
    }
    cJSON *params = NULL;
    for (cJSON *dev = root->child; dev != NULL; dev = dev->next) {
        if (cJSON_IsObject(dev)) {
            params = dev;
            break;
        }
    }
    if (!params) {
        cJSON_Delete(root);
        return;
    }

    switch (handle->type) {
    case RAINMAKER_APP_DEVICE_TYPE_LIGHT: {
        if (handle->whether_has_power) {
            cJSON *power = cJSON_GetObjectItem(params, RM_STANDARD_DEVICE_POWER);
            if (cJSON_IsBool(power)) {
                s_select_actions_ctx.state.power_on = cJSON_IsTrue(power);
                s_select_actions_ctx.state.power_set = true;
            }
        }
        cJSON *brightness = cJSON_GetObjectItem(params, RM_STANDARD_LIGHT_DEVICE_BRIGHTNESS);
        if (cJSON_IsNumber(brightness)) {
            s_select_actions_ctx.state.product.light.brightness = (int)cJSON_GetNumberValue(brightness);
            s_select_actions_ctx.state.set.brightness = true;
        }
        cJSON *hue = cJSON_GetObjectItem(params, RM_STANDARD_LIGHT_DEVICE_HUE);
        if (cJSON_IsNumber(hue)) {
            s_select_actions_ctx.state.product.light.hue = (int)cJSON_GetNumberValue(hue);
            s_select_actions_ctx.state.set.hue = true;
        }
        cJSON *saturation = cJSON_GetObjectItem(params, RM_STANDARD_LIGHT_DEVICE_SATURATION);
        if (cJSON_IsNumber(saturation)) {
            s_select_actions_ctx.state.product.light.saturation = (int)cJSON_GetNumberValue(saturation);
            s_select_actions_ctx.state.set.saturation = true;
        }
        break;
    }
    case RAINMAKER_APP_DEVICE_TYPE_SWITCH:
        if (handle->whether_has_power) {
            cJSON *power = cJSON_GetObjectItem(params, RM_STANDARD_DEVICE_POWER);
            if (cJSON_IsBool(power)) {
                s_select_actions_ctx.state.power_on = cJSON_IsTrue(power);
                s_select_actions_ctx.state.power_set = true;
            }
        }
        break;
    case RAINMAKER_APP_DEVICE_TYPE_FAN: {
        if (handle->whether_has_power) {
            cJSON *power = cJSON_GetObjectItem(params, RM_STANDARD_DEVICE_POWER);
            if (cJSON_IsBool(power)) {
                s_select_actions_ctx.state.power_on = cJSON_IsTrue(power);
                s_select_actions_ctx.state.power_set = true;
            }
        }
        cJSON *speed = cJSON_GetObjectItem(params, RM_STANDARD_FAN_DEVICE_SPEED);
        if (cJSON_IsNumber(speed)) {
            s_select_actions_ctx.state.product.fan.speed = (int)cJSON_GetNumberValue(speed);
            s_select_actions_ctx.state.set.speed = true;
        }
        break;
    }
    default: /* RAINMAKER_APP_DEVICE_TYPE_OTHER and unknown */
        if (handle->whether_has_power) {
            cJSON *power = cJSON_GetObjectItem(params, RM_STANDARD_DEVICE_POWER);
            if (cJSON_IsBool(power)) {
                s_select_actions_ctx.state.power_on = cJSON_IsTrue(power);
                s_select_actions_ctx.state.power_set = true;
            }
        }
        break;
    }

    cJSON_Delete(root);
}

/** Set each row's value_label from current s_power_on, s_brightness, etc. */
static void select_actions_set_initial_value_labels(void)
{
    if (!s_actions_list) {
        return;
    }
    uint32_t cnt = lv_obj_get_child_cnt(s_actions_list);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *row = lv_obj_get_child(s_actions_list, i);
        if (!row) {
            continue;
        }
        row_ctx_t *rctx = (row_ctx_t *)lv_obj_get_user_data(row);
        lv_obj_t *value_label = lv_obj_get_child(row, 1);
        if (!rctx || !value_label) {
            continue;
        }
        const char *action = rctx->action;
        if (!action) {
            continue;
        }
        if (strcmp(action, RM_STANDARD_DEVICE_POWER) == 0) {
            lv_label_set_text(value_label,
                              s_select_actions_ctx.state.power_set
                              ? (s_select_actions_ctx.state.power_on ? ui_str(UI_STR_ON) : ui_str(UI_STR_OFF))
                              : "");
        } else if (strcmp(action, RM_STANDARD_LIGHT_DEVICE_BRIGHTNESS) == 0) {
            if (s_select_actions_ctx.state.set.brightness) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", s_select_actions_ctx.state.product.light.brightness);
                lv_label_set_text(value_label, buf);
            } else {
                lv_label_set_text(value_label, "");
            }
        } else if (strcmp(action, RM_STANDARD_LIGHT_DEVICE_HUE) == 0) {
            if (s_select_actions_ctx.state.set.hue) {
                char buf[24];
                ui_format_hue_value_string(s_select_actions_ctx.state.product.light.hue, buf, sizeof(buf));
                lv_label_set_text(value_label, buf);
            } else {
                lv_label_set_text(value_label, "");
            }
        } else if (strcmp(action, RM_STANDARD_LIGHT_DEVICE_SATURATION) == 0) {
            if (s_select_actions_ctx.state.set.saturation) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d%%", s_select_actions_ctx.state.product.light.saturation);
                lv_label_set_text(value_label, buf);
            } else {
                lv_label_set_text(value_label, "");
            }
        } else if (strcmp(action, RM_STANDARD_FAN_DEVICE_SPEED) == 0) {
            if (s_select_actions_ctx.state.set.speed) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", s_select_actions_ctx.state.product.fan.speed);
                lv_label_set_text(value_label, buf);
            } else {
                lv_label_set_text(value_label, "");
            }
        }
    }
}

static void select_actions_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (s_select_actions_ctx.edit_item != NULL) {
        bool is_scene_edit = (s_select_actions_ctx.edit_item->entity_type == RM_LIST_ENTITY_TYPE_SCENE);
        s_select_actions_ctx.edit_item = NULL;
        s_select_actions_ctx.edit_action_index = -1;
        if (is_scene_edit && ui_Screen_CreateScene) {
            lv_scr_load(ui_Screen_CreateScene);
        } else if (ui_Screen_CreateSchedule) {
            lv_scr_load(ui_Screen_CreateSchedule);
        }
    } else if (ui_Screen_SelectDevices) {
        lv_scr_load(ui_Screen_SelectDevices);
    }
}

static void select_actions_done_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    char *summary = NULL;
    size_t cap = 0;
    size_t used = 0;
    bool first_line = true;
    if (s_actions_list) {
        uint32_t cnt = lv_obj_get_child_cnt(s_actions_list);
        for (uint32_t i = 0; i < cnt; i++) {
            lv_obj_t *row = lv_obj_get_child(s_actions_list, i);
            if (!row) {
                continue;
            }
            row_ctx_t *rctx = (row_ctx_t *)lv_obj_get_user_data(row);
            lv_obj_t *value_label = lv_obj_get_child(row, 1);
            if (!rctx || !value_label) {
                continue;
            }
            const char *action = rctx->action;
            const char *value = lv_label_get_text(value_label);
            if (!action) {
                continue;
            }
            /* Summary must use English "On"/"Off" for Power so rm_list_manager_summary_to_action_json can
             * parse it; the label may show localized UI_STR_ON/OFF. */
            if (strcmp(action, RM_STANDARD_DEVICE_POWER) == 0) {
                if (!s_select_actions_ctx.state.power_set) {
                    continue;
                }
                value = s_select_actions_ctx.state.power_on ? "On" : "Off";
            } else if (!value || value[0] == '\0') {
                continue;
            }
            int need = snprintf(NULL, 0, "%s%s to %s", first_line ? "" : "\n", action, value);
            if (need <= 0) {
                continue;
            }
            size_t need_len = (size_t)need + 1;
            if (used + need_len > cap) {
                size_t new_cap = (cap == 0) ? (need_len + 32) : (cap * 2);
                while (new_cap < used + need_len) {
                    new_cap *= 2;
                }
                char *new_summary = (char *)realloc(summary, new_cap);
                if (new_summary == NULL) {
                    break;
                }
                summary = new_summary;
                cap = new_cap;
            }
            int written = snprintf(summary + used, cap - used, "%s%s to %s",
                                   first_line ? "" : "\n", action, value);
            if (written <= 0 || (size_t)written >= cap - used) {
                break;
            }
            used += (size_t)written;
            first_line = false;
        }
    }
    if (s_select_actions_ctx.edit_item != NULL && s_select_actions_ctx.edit_action_index >= 0) {
        /* Edit mode: update the list's node action and return to CreateSchedule or CreateScene */
        rm_node_action_item_t *node_action_item = rm_list_manager_get_node_action_item_by_index(s_select_actions_ctx.edit_item, s_select_actions_ctx.edit_action_index);
        if (node_action_item && s_select_actions_ctx.handle) {
            char *action_json = NULL;
            size_t action_json_size = 0;
            if (summary && used > 0) {
                summary[used] = '\0';
                rm_list_manager_summary_to_action_json(s_select_actions_ctx.handle->type, summary, &action_json, &action_json_size);
            }
            rainmaker_app_backend_util_safe_free((void *)node_action_item->action);
            node_action_item->action = (action_json != NULL) ? action_json : strdup("");
        }
        free(summary);
        bool is_scene_edit = (s_select_actions_ctx.edit_item &&
                              s_select_actions_ctx.edit_item->entity_type == RM_LIST_ENTITY_TYPE_SCENE);
        s_select_actions_ctx.edit_item = NULL;
        s_select_actions_ctx.edit_action_index = -1;
        if (is_scene_edit && ui_Screen_CreateScene) {
            lv_scr_load(ui_Screen_CreateScene);
            ui_Screen_CreateScene_refresh_actions();
        } else if (ui_Screen_CreateSchedule) {
            lv_scr_load(ui_Screen_CreateSchedule);
            ui_Screen_CreateSchedule_refresh_actions();
        }
    } else {
        if (summary && used > 0) {
            summary[used] = '\0';
            ui_Screen_SelectDevices_set_selected_summary(s_select_actions_ctx.handle, summary);
        } else {
            ui_Screen_SelectDevices_set_selected_summary(s_select_actions_ctx.handle, NULL);
        }
        free(summary);
        ui_Screen_SelectDevices_refresh();
        lv_scr_load(ui_Screen_SelectDevices);
    }
}

static void select_actions_row_delete_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }
    row_ctx_t *ctx = (row_ctx_t *)lv_event_get_user_data(e);
    if (ctx) {
        UI_LV_FREE(ctx);
    }
}

static void power_save_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *sheet = (lv_obj_t *)lv_event_get_user_data(e);
    if (!sheet) {
        return;
    }
    lv_obj_t *panel = lv_obj_get_child(sheet, 0);
    lv_obj_t *value_label = (lv_obj_t *)lv_obj_get_user_data(panel);
    lv_obj_t *row = lv_obj_get_child(panel, 1);
    lv_obj_t *sw = row ? lv_obj_get_child(row, 1) : NULL;
    if (sw) {
        s_select_actions_ctx.state.power_on = lv_obj_has_state(sw, LV_STATE_CHECKED);
        s_select_actions_ctx.state.power_set = true;
        if (value_label) {
            lv_label_set_text(value_label, s_select_actions_ctx.state.power_on ? ui_str(UI_STR_ON) : ui_str(UI_STR_OFF));
        }
    }
    lv_obj_del(sheet);
}

static void power_cancel_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *sheet = (lv_obj_t *)lv_event_get_user_data(e);
    if (sheet) {
        lv_obj_del(sheet);
    }
}

static void power_delete_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *sheet = (lv_obj_t *)lv_event_get_user_data(e);
    if (!sheet) {
        return;
    }
    lv_obj_t *panel = lv_obj_get_child(sheet, 0);
    lv_obj_t *value_label = (lv_obj_t *)lv_obj_get_user_data(panel);
    if (value_label) {
        lv_label_set_text(value_label, "");
    }
    s_select_actions_ctx.state.power_on = false;
    s_select_actions_ctx.state.power_set = false;
    lv_obj_del(sheet);
}

static void brightness_slider_changed_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    lv_obj_t *lab = (lv_obj_t *)lv_event_get_user_data(e);
    if (lab) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", (int)lv_slider_get_value(slider));
        lv_label_set_text(lab, buf);
    }
}

static void hue_slider_changed_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    lv_obj_t *lab = (lv_obj_t *)lv_event_get_user_data(e);
    if (lab) {
        char buf[24];
        ui_format_hue_value_string((int)lv_slider_get_value(slider), buf, sizeof(buf));
        lv_label_set_text(lab, buf);
    }
}

static void saturation_slider_changed_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    lv_obj_t *lab = (lv_obj_t *)lv_event_get_user_data(e);
    if (lab) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", (int)lv_slider_get_value(slider));
        lv_label_set_text(lab, buf);
    }
}

static void speed_slider_changed_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    lv_obj_t *lab = (lv_obj_t *)lv_event_get_user_data(e);
    if (lab) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", (int)lv_slider_get_value(slider));
        lv_label_set_text(lab, buf);
    }
}

static void brightness_save_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *save_btn = lv_event_get_target(e);
    lv_obj_t *btn_row = lv_obj_get_parent(save_btn);
    lv_obj_t *panel = btn_row ? lv_obj_get_parent(btn_row) : NULL;
    lv_obj_t *sheet = panel ? lv_obj_get_parent(panel) : NULL;
    lv_obj_t *value_label = panel ? (lv_obj_t *)lv_obj_get_user_data(panel) : NULL;
    lv_obj_t *slider_row = panel ? lv_obj_get_child(panel, 2) : NULL;
    lv_obj_t *slider = slider_row ? select_actions_slider_row_find_slider(slider_row) : NULL;
    if (slider && value_label) {
        s_select_actions_ctx.state.product.light.brightness = (int)lv_slider_get_value(slider);
        s_select_actions_ctx.state.set.brightness = true;
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", s_select_actions_ctx.state.product.light.brightness);
        lv_label_set_text(value_label, buf);
    }
    if (sheet) {
        lv_obj_del(sheet);
    }
}

static void brightness_cancel_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *sheet = (lv_obj_t *)lv_event_get_user_data(e);
    if (sheet) {
        lv_obj_del(sheet);
    }
}

static void brightness_delete_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *sheet = (lv_obj_t *)lv_event_get_user_data(e);
    if (!sheet) {
        return;
    }
    lv_obj_t *panel = lv_obj_get_child(sheet, 0);
    lv_obj_t *value_label = (lv_obj_t *)lv_obj_get_user_data(panel);
    if (value_label) {
        lv_label_set_text(value_label, "");
    }
    s_select_actions_ctx.state.product.light.brightness = 0;
    s_select_actions_ctx.state.set.brightness = false;
    lv_obj_del(sheet);
}

static void hue_save_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *save_btn = lv_event_get_target(e);
    lv_obj_t *btn_row = lv_obj_get_parent(save_btn);
    lv_obj_t *panel = btn_row ? lv_obj_get_parent(btn_row) : NULL;
    lv_obj_t *sheet = panel ? lv_obj_get_parent(panel) : NULL;
    lv_obj_t *value_label = panel ? (lv_obj_t *)lv_obj_get_user_data(panel) : NULL;
    lv_obj_t *slider_row = panel ? lv_obj_get_child(panel, 2) : NULL;
    lv_obj_t *slider = slider_row ? select_actions_slider_row_find_slider(slider_row) : NULL;
    if (slider && value_label) {
        s_select_actions_ctx.state.product.light.hue = (int)lv_slider_get_value(slider);
        s_select_actions_ctx.state.set.hue = true;
        char buf[24];
        ui_format_hue_value_string(s_select_actions_ctx.state.product.light.hue, buf, sizeof(buf));
        lv_label_set_text(value_label, buf);
    }
    if (sheet) {
        lv_obj_del(sheet);
    }
}

static void hue_cancel_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *sheet = (lv_obj_t *)lv_event_get_user_data(e);
    if (sheet) {
        lv_obj_del(sheet);
    }
}

static void hue_delete_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *sheet = (lv_obj_t *)lv_event_get_user_data(e);
    if (!sheet) {
        return;
    }
    lv_obj_t *panel = lv_obj_get_child(sheet, 0);
    lv_obj_t *value_label = (lv_obj_t *)lv_obj_get_user_data(panel);
    if (value_label) {
        lv_label_set_text(value_label, "");
    }
    s_select_actions_ctx.state.product.light.hue = 0;
    s_select_actions_ctx.state.set.hue = false;
    lv_obj_del(sheet);
}

static void saturation_save_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *save_btn = lv_event_get_target(e);
    lv_obj_t *btn_row = lv_obj_get_parent(save_btn);
    lv_obj_t *panel = btn_row ? lv_obj_get_parent(btn_row) : NULL;
    lv_obj_t *sheet = panel ? lv_obj_get_parent(panel) : NULL;
    lv_obj_t *value_label = panel ? (lv_obj_t *)lv_obj_get_user_data(panel) : NULL;
    lv_obj_t *slider_row = panel ? lv_obj_get_child(panel, 2) : NULL;
    lv_obj_t *slider = slider_row ? select_actions_slider_row_find_slider(slider_row) : NULL;
    if (slider && value_label) {
        s_select_actions_ctx.state.product.light.saturation = (int)lv_slider_get_value(slider);
        s_select_actions_ctx.state.set.saturation = true;
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", s_select_actions_ctx.state.product.light.saturation);
        lv_label_set_text(value_label, buf);
    }
    if (sheet) {
        lv_obj_del(sheet);
    }
}

static void saturation_cancel_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *sheet = (lv_obj_t *)lv_event_get_user_data(e);
    if (sheet) {
        lv_obj_del(sheet);
    }
}

static void saturation_delete_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *sheet = (lv_obj_t *)lv_event_get_user_data(e);
    if (!sheet) {
        return;
    }
    lv_obj_t *panel = lv_obj_get_child(sheet, 0);
    lv_obj_t *value_label = (lv_obj_t *)lv_obj_get_user_data(panel);
    if (value_label) {
        lv_label_set_text(value_label, "");
    }
    s_select_actions_ctx.state.product.light.saturation = 0;
    s_select_actions_ctx.state.set.saturation = false;
    lv_obj_del(sheet);
}

static void speed_save_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *save_btn = lv_event_get_target(e);
    lv_obj_t *btn_row = lv_obj_get_parent(save_btn);
    lv_obj_t *panel = btn_row ? lv_obj_get_parent(btn_row) : NULL;
    lv_obj_t *sheet = panel ? lv_obj_get_parent(panel) : NULL;
    lv_obj_t *value_label = panel ? (lv_obj_t *)lv_obj_get_user_data(panel) : NULL;
    lv_obj_t *slider_row = panel ? lv_obj_get_child(panel, 2) : NULL;
    lv_obj_t *slider = slider_row ? select_actions_slider_row_find_slider(slider_row) : NULL;
    if (slider && value_label) {
        s_select_actions_ctx.state.product.fan.speed = (int)lv_slider_get_value(slider);
        s_select_actions_ctx.state.set.speed = true;
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", s_select_actions_ctx.state.product.fan.speed);
        lv_label_set_text(value_label, buf);
    }
    if (sheet) {
        lv_obj_del(sheet);
    }
}

static void speed_cancel_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *sheet = (lv_obj_t *)lv_event_get_user_data(e);
    if (sheet) {
        lv_obj_del(sheet);
    }
}

static void speed_delete_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *sheet = (lv_obj_t *)lv_event_get_user_data(e);
    if (!sheet) {
        return;
    }
    lv_obj_t *panel = lv_obj_get_child(sheet, 0);
    lv_obj_t *value_label = (lv_obj_t *)lv_obj_get_user_data(panel);
    if (value_label) {
        lv_label_set_text(value_label, "");
    }
    s_select_actions_ctx.state.product.fan.speed = 0;
    s_select_actions_ctx.state.set.speed = false;
    lv_obj_del(sheet);
}

static void show_power_sheet(lv_obj_t *value_label)
{
    const char *cur = value_label ? lv_label_get_text(value_label) : NULL;
    bool has_value = (cur && cur[0] != '\0');

    lv_obj_t *scr = ui_Screen_SelectActions;
    lv_obj_t *sheet = lv_obj_create(scr);
    lv_obj_clear_flag(sheet, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(sheet, lv_pct(100), lv_pct(100));
    lv_obj_align(sheet, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(sheet, LV_OPA_0, 0);
    lv_obj_set_style_border_width(sheet, 0, 0);

    lv_obj_t *panel = lv_obj_create(sheet);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(panel, lv_pct(100));
    lv_obj_set_style_min_height(panel, 200, 0);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 20, 0);
    lv_obj_set_style_pad_top(panel, 12, 0);
    lv_obj_set_style_pad_bottom(panel, 20, 0);
    lv_obj_set_user_data(panel, value_label);

    /* Top handle: light blue-grey rounded bar */
    lv_obj_t *handle = lv_obj_create(panel);
    lv_obj_clear_flag(handle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(handle, 40, 5);
    lv_obj_set_style_radius(handle, 3, 0);
    lv_obj_set_style_bg_color(handle, lv_color_hex(0xB8C4D0), 0);
    lv_obj_set_style_border_width(handle, 0, 0);
    lv_obj_align(handle, LV_ALIGN_TOP_MID, 0, 0);

    /* Row: Power label (left) + toggle (right), vertically centered in content area */
    lv_obj_t *row = lv_obj_create(panel);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, 48);
    lv_obj_set_style_bg_opa(row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align_to(row, handle, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);

    lv_obj_t *title = lv_label_create(row);
    lv_label_set_text(title, ui_str(UI_STR_POWER));
    lv_obj_set_style_text_font(title, ui_font_body(), 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x2D3748), 0);

    lv_obj_t *sw = lv_switch_create(row);
    lv_obj_set_size(sw, 48, 26);
    lv_obj_set_style_radius(sw, 13, LV_PART_MAIN);
    lv_obj_set_style_radius(sw, 13, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0xE2E8F0), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0xCFE0FF), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x2E6BE6), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_radius(sw, 12, LV_PART_KNOB);
    lv_obj_set_style_width(sw, 22, LV_PART_KNOB);
    lv_obj_set_style_height(sw, 22, LV_PART_KNOB);
    lv_obj_set_style_pad_all(sw, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x2E6BE6), LV_PART_KNOB | LV_STATE_CHECKED);
    if (s_select_actions_ctx.state.power_on) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }

    /* Button row: Cancel (left) + Save (right) */
    lv_obj_t *btn_row = lv_obj_create(panel);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(btn_row, lv_pct(100));
    lv_obj_set_height(btn_row, 48);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_left(btn_row, 8, 0);
    lv_obj_set_style_pad_right(btn_row, 8, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn_row, 12, 0);
    lv_obj_align_to(btn_row, row, LV_ALIGN_OUT_BOTTOM_MID, 0, 24);

    lv_obj_t *left_btn = lv_btn_create(btn_row);
    lv_obj_clear_flag(left_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_height(left_btn, 48);
    lv_obj_set_flex_grow(left_btn, 1);
    lv_obj_set_style_radius(left_btn, 12, 0);
    lv_obj_set_style_border_width(left_btn, 0, 0);
    if (has_value) {
        lv_obj_set_style_bg_color(left_btn, lv_color_hex(0xDC2626), 0);
        lv_obj_set_style_bg_color(left_btn, lv_color_hex(0xB91C1C), LV_STATE_PRESSED);
        lv_obj_add_event_cb(left_btn, power_delete_cb, LV_EVENT_CLICKED, sheet);
    } else {
        lv_obj_set_style_bg_color(left_btn, lv_color_hex(0xE8ECF0), 0);
        lv_obj_add_event_cb(left_btn, power_cancel_cb, LV_EVENT_CLICKED, sheet);
    }
    lv_obj_t *left_lab = lv_label_create(left_btn);
    lv_label_set_text(left_lab, has_value ? ui_str(UI_STR_DELETE) : ui_str(UI_STR_CANCEL));
    lv_obj_set_style_text_font(left_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(left_lab, lv_color_hex(has_value ? 0xFFFFFF : 0x4A5568), 0);
    lv_obj_center(left_lab);

    lv_obj_t *save_btn = lv_btn_create(btn_row);
    lv_obj_clear_flag(save_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_height(save_btn, 48);
    lv_obj_set_flex_grow(save_btn, 1);
    lv_obj_set_style_radius(save_btn, 12, 0);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x2E6BE6), 0);
    lv_obj_set_style_border_width(save_btn, 0, 0);
    lv_obj_add_event_cb(save_btn, power_save_cb, LV_EVENT_CLICKED, sheet);
    lv_obj_t *save_lab = lv_label_create(save_btn);
    lv_label_set_text(save_lab, ui_str(UI_STR_SAVE));
    lv_obj_set_style_text_font(save_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(save_lab, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(save_lab);

    lv_obj_move_foreground(sheet);
}

/* Style slider like Fan/Light detail: grey track, blue fill from left to value, circular white knob */
static void select_actions_style_brightness_slider(lv_obj_t *slider)
{
    const int32_t track_h = 6;
    const int32_t knob_d = 26;
    const int32_t knob_half = knob_d / 2;
    const int32_t knob_pad = (knob_d - track_h) / 2;
    /* Ensure slider has width so indicator (0 to value) is drawn correctly */
    lv_obj_set_style_min_width(slider, 80, 0);
    /* MAIN: track background (grey) */
    lv_obj_set_style_height(slider, track_h, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, track_h / 2, LV_PART_MAIN);
    lv_obj_set_style_pad_left(slider, knob_half, LV_PART_MAIN);
    lv_obj_set_style_pad_right(slider, knob_half, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xE2E8F0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_90, LV_PART_MAIN);
    /* INDICATOR: blue from left (0) to current value; pad_left 0 so 0 is filled when value > 0 */
    lv_obj_set_style_height(slider, track_h, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, track_h / 2, LV_PART_INDICATOR);
    lv_obj_set_style_pad_left(slider, 0, LV_PART_INDICATOR);
    lv_obj_set_style_pad_right(slider, knob_half, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x2E6BE6), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_90, LV_PART_INDICATOR);
    /* KNOB: white circle */
    lv_obj_set_style_width(slider, knob_d, LV_PART_KNOB);
    lv_obj_set_style_height(slider, knob_d, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, knob_pad, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_border_width(slider, 2, LV_PART_KNOB);
    lv_obj_set_style_border_color(slider, lv_color_hex(0xD0D7E2), LV_PART_KNOB);
}

static void show_brightness_sheet(lv_obj_t *value_label)
{
    const char *cur = value_label ? lv_label_get_text(value_label) : NULL;
    bool has_value = (cur && cur[0] != '\0');

    lv_obj_t *scr = ui_Screen_SelectActions;
    lv_obj_t *sheet = lv_obj_create(scr);
    lv_obj_clear_flag(sheet, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(sheet, lv_pct(100), lv_pct(100));
    lv_obj_align(sheet, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(sheet, LV_OPA_0, 0);
    lv_obj_set_style_border_width(sheet, 0, 0);

    lv_obj_t *panel = lv_obj_create(sheet);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(panel, lv_pct(100));
    lv_obj_set_style_min_height(panel, 220, 0);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 20, 0);
    lv_obj_set_style_pad_top(panel, 12, 0);
    lv_obj_set_style_pad_bottom(panel, 20, 0);
    lv_obj_set_user_data(panel, value_label);

    lv_obj_t *handle = lv_obj_create(panel);
    lv_obj_clear_flag(handle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(handle, 40, 5);
    lv_obj_set_style_radius(handle, 3, 0);
    lv_obj_set_style_bg_color(handle, lv_color_hex(0xB8C4D0), 0);
    lv_obj_set_style_border_width(handle, 0, 0);
    lv_obj_align(handle, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *title_row = lv_obj_create(panel);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(title_row, lv_pct(90));
    lv_obj_set_height(title_row, 24);
    lv_obj_set_style_bg_opa(title_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_set_flex_flow(title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align_to(title_row, panel, LV_ALIGN_TOP_MID, 0, 33);

    lv_obj_t *title = lv_label_create(title_row);
    lv_label_set_text(title, ui_str(UI_STR_BRIGHTNESS));
    lv_obj_set_style_text_font(title, ui_font_body(), 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x2D3748), 0);

    lv_obj_t *val_lab = lv_label_create(title_row);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", s_select_actions_ctx.state.product.light.brightness);
    lv_label_set_text(val_lab, buf);
    lv_obj_set_style_text_font(val_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(val_lab, lv_color_hex(0x718096), 0);

    lv_obj_t *slider_row = lv_obj_create(panel);
    lv_obj_clear_flag(slider_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(slider_row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_width(slider_row, lv_pct(90));
    lv_obj_set_height(slider_row, 52);
    lv_obj_set_style_bg_opa(slider_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(slider_row, 0, 0);
    lv_obj_align_to(slider_row, title_row, LV_ALIGN_OUT_BOTTOM_MID, 0, 16);

    lv_obj_t *slider = lv_slider_create(slider_row);
    lv_obj_set_width(slider, lv_pct(100));
    lv_obj_set_height(slider, 26);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(slider, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, s_select_actions_ctx.state.product.light.brightness, LV_ANIM_OFF);
    select_actions_style_brightness_slider(slider);
    lv_obj_add_event_cb(slider, brightness_slider_changed_cb, LV_EVENT_VALUE_CHANGED, val_lab);

    lv_obj_t *btn_row = lv_obj_create(panel);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(btn_row, lv_pct(90));
    lv_obj_set_height(btn_row, 48);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_left(btn_row, 8, 0);
    lv_obj_set_style_pad_right(btn_row, 8, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn_row, 12, 0);
    lv_obj_align_to(btn_row, slider_row, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);

    lv_obj_t *left_btn = lv_btn_create(btn_row);
    lv_obj_clear_flag(left_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_height(left_btn, 48);
    lv_obj_set_flex_grow(left_btn, 1);
    lv_obj_set_style_radius(left_btn, 12, 0);
    lv_obj_set_style_border_width(left_btn, 0, 0);
    if (has_value) {
        lv_obj_set_style_bg_color(left_btn, lv_color_hex(0xDC2626), 0);
        lv_obj_set_style_bg_color(left_btn, lv_color_hex(0xB91C1C), LV_STATE_PRESSED);
        lv_obj_add_event_cb(left_btn, brightness_delete_cb, LV_EVENT_CLICKED, sheet);
    } else {
        lv_obj_set_style_bg_color(left_btn, lv_color_hex(0xE8ECF0), 0);
        lv_obj_add_event_cb(left_btn, brightness_cancel_cb, LV_EVENT_CLICKED, sheet);
    }
    lv_obj_t *left_lab = lv_label_create(left_btn);
    lv_label_set_text(left_lab, has_value ? ui_str(UI_STR_DELETE) : ui_str(UI_STR_CANCEL));
    lv_obj_set_style_text_font(left_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(left_lab, lv_color_hex(has_value ? 0xFFFFFF : 0x4A5568), 0);
    lv_obj_center(left_lab);

    lv_obj_t *save_btn = lv_btn_create(btn_row);
    lv_obj_clear_flag(save_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_height(save_btn, 48);
    lv_obj_set_flex_grow(save_btn, 1);
    lv_obj_set_style_radius(save_btn, 12, 0);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x2E6BE6), 0);
    lv_obj_set_style_border_width(save_btn, 0, 0);
    lv_obj_add_event_cb(save_btn, brightness_save_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *save_lab = lv_label_create(save_btn);
    lv_label_set_text(save_lab, ui_str(UI_STR_SAVE));
    lv_obj_set_style_text_font(save_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(save_lab, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(save_lab);

    lv_obj_move_foreground(sheet);
}

static void show_hue_sheet(lv_obj_t *value_label)
{
    const char *cur = value_label ? lv_label_get_text(value_label) : NULL;
    bool has_value = (cur && cur[0] != '\0');
    if (has_value) {
        int v = atoi(cur);
        if (v >= 0 && v <= 360) {
            s_select_actions_ctx.state.product.light.hue = v;
        }
    }

    lv_obj_t *scr = ui_Screen_SelectActions;
    lv_obj_t *sheet = lv_obj_create(scr);
    lv_obj_clear_flag(sheet, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(sheet, lv_pct(100), lv_pct(100));
    lv_obj_align(sheet, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(sheet, LV_OPA_0, 0);
    lv_obj_set_style_border_width(sheet, 0, 0);

    lv_obj_t *panel = lv_obj_create(sheet);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(panel, lv_pct(100));
    lv_obj_set_style_min_height(panel, 220, 0);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 20, 0);
    lv_obj_set_style_pad_top(panel, 12, 0);
    lv_obj_set_style_pad_bottom(panel, 20, 0);
    lv_obj_set_user_data(panel, value_label);

    lv_obj_t *handle = lv_obj_create(panel);
    lv_obj_clear_flag(handle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(handle, 40, 5);
    lv_obj_set_style_radius(handle, 3, 0);
    lv_obj_set_style_bg_color(handle, lv_color_hex(0xB8C4D0), 0);
    lv_obj_set_style_border_width(handle, 0, 0);
    lv_obj_align(handle, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *title_row = lv_obj_create(panel);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(title_row, lv_pct(90));
    lv_obj_set_height(title_row, 24);
    lv_obj_set_style_bg_opa(title_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_set_flex_flow(title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align_to(title_row, panel, LV_ALIGN_TOP_MID, 0, 33);

    lv_obj_t *title = lv_label_create(title_row);
    lv_label_set_text(title, ui_str(UI_STR_HUE));
    lv_obj_set_style_text_font(title, ui_font_body(), 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x2D3748), 0);

    lv_obj_t *val_lab = lv_label_create(title_row);
    char buf[24];
    ui_format_hue_value_string(s_select_actions_ctx.state.product.light.hue, buf, sizeof(buf));
    lv_label_set_text(val_lab, buf);
    lv_obj_set_style_text_font(val_lab, ui_i18n_uses_cjk() ? ui_font_cjk_always() : ui_font_body(), 0);
    lv_obj_set_style_text_color(val_lab, lv_color_hex(0x718096), 0);

    const int32_t knob_half = SELECT_ACTIONS_LIGHT_SLIDER_KNOB_D / 2;
    const int32_t h_slider_row_h = SELECT_ACTIONS_LIGHT_SLIDER_KNOB_D + 8;
    lv_obj_t *slider_row = lv_obj_create(panel);
    lv_obj_clear_flag(slider_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(slider_row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_width(slider_row, lv_pct(90));
    lv_obj_set_height(slider_row, h_slider_row_h);
    lv_obj_set_style_bg_opa(slider_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(slider_row, 0, 0);
    lv_obj_align_to(slider_row, title_row, LV_ALIGN_OUT_BOTTOM_MID, 0, 16);

    lv_obj_t *hue_bar = lv_obj_create(slider_row);
    lv_obj_remove_style_all(hue_bar);
    lv_obj_clear_flag(hue_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(hue_bar, lv_pct(100));
    lv_obj_set_height(hue_bar, SELECT_ACTIONS_LIGHT_SLIDER_TRACK_H);
    lv_obj_set_style_pad_left(hue_bar, knob_half, 0);
    lv_obj_set_style_pad_right(hue_bar, knob_half, 0);
    lv_obj_set_style_bg_opa(hue_bar, LV_OPA_0, 0);
    lv_obj_set_style_border_width(hue_bar, 0, 0);
    lv_obj_set_style_radius(hue_bar, SELECT_ACTIONS_LIGHT_SLIDER_TRACK_H / 2, 0);
    lv_obj_set_style_clip_corner(hue_bar, true, 0);
    lv_obj_align(hue_bar, LV_ALIGN_CENTER, 0, 0);
    select_actions_hue_rainbow_ensure_grad();
    lv_obj_set_style_bg_grad(hue_bar, &s_sa_hue_rainbow_grad, 0);
    lv_obj_set_style_bg_opa(hue_bar, LV_OPA_COVER, 0);

    lv_obj_t *slider = lv_slider_create(slider_row);
    lv_obj_set_width(slider, lv_pct(100));
    lv_obj_set_height(slider, SELECT_ACTIONS_LIGHT_SLIDER_KNOB_D);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(slider, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_slider_set_range(slider, 0, 360);
    lv_slider_set_value(slider, s_select_actions_ctx.state.product.light.hue, LV_ANIM_OFF);
    select_actions_style_light_hue_slider_knob_only(slider);
    lv_obj_add_event_cb(slider, hue_slider_changed_cb, LV_EVENT_VALUE_CHANGED, val_lab);
    lv_obj_move_foreground(slider);

    lv_obj_t *btn_row = lv_obj_create(panel);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(btn_row, lv_pct(90));
    lv_obj_set_height(btn_row, 48);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_left(btn_row, 8, 0);
    lv_obj_set_style_pad_right(btn_row, 8, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn_row, 12, 0);
    lv_obj_align_to(btn_row, slider_row, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);

    lv_obj_t *left_btn = lv_btn_create(btn_row);
    lv_obj_clear_flag(left_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_height(left_btn, 48);
    lv_obj_set_flex_grow(left_btn, 1);
    lv_obj_set_style_radius(left_btn, 12, 0);
    lv_obj_set_style_border_width(left_btn, 0, 0);
    if (has_value) {
        lv_obj_set_style_bg_color(left_btn, lv_color_hex(0xDC2626), 0);
        lv_obj_set_style_bg_color(left_btn, lv_color_hex(0xB91C1C), LV_STATE_PRESSED);
        lv_obj_add_event_cb(left_btn, hue_delete_cb, LV_EVENT_CLICKED, sheet);
    } else {
        lv_obj_set_style_bg_color(left_btn, lv_color_hex(0xE8ECF0), 0);
        lv_obj_add_event_cb(left_btn, hue_cancel_cb, LV_EVENT_CLICKED, sheet);
    }
    lv_obj_t *left_lab = lv_label_create(left_btn);
    lv_label_set_text(left_lab, has_value ? ui_str(UI_STR_DELETE) : ui_str(UI_STR_CANCEL));
    lv_obj_set_style_text_font(left_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(left_lab, lv_color_hex(has_value ? 0xFFFFFF : 0x4A5568), 0);
    lv_obj_center(left_lab);

    lv_obj_t *save_btn = lv_btn_create(btn_row);
    lv_obj_clear_flag(save_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_height(save_btn, 48);
    lv_obj_set_flex_grow(save_btn, 1);
    lv_obj_set_style_radius(save_btn, 12, 0);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x2E6BE6), 0);
    lv_obj_set_style_border_width(save_btn, 0, 0);
    lv_obj_add_event_cb(save_btn, hue_save_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *save_lab = lv_label_create(save_btn);
    lv_label_set_text(save_lab, ui_str(UI_STR_SAVE));
    lv_obj_set_style_text_font(save_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(save_lab, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(save_lab);

    lv_obj_move_foreground(sheet);
}

static void show_saturation_sheet(lv_obj_t *value_label)
{
    const char *cur = value_label ? lv_label_get_text(value_label) : NULL;
    bool has_value = (cur && cur[0] != '\0');
    if (has_value) {
        int v = atoi(cur);
        if (v >= 0 && v <= 100) {
            s_select_actions_ctx.state.product.light.saturation = v;
        }
    }

    lv_obj_t *scr = ui_Screen_SelectActions;
    lv_obj_t *sheet = lv_obj_create(scr);
    lv_obj_clear_flag(sheet, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(sheet, lv_pct(100), lv_pct(100));
    lv_obj_align(sheet, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(sheet, LV_OPA_0, 0);
    lv_obj_set_style_border_width(sheet, 0, 0);

    lv_obj_t *panel = lv_obj_create(sheet);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(panel, lv_pct(100));
    lv_obj_set_style_min_height(panel, 220, 0);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 20, 0);
    lv_obj_set_style_pad_top(panel, 12, 0);
    lv_obj_set_style_pad_bottom(panel, 20, 0);
    lv_obj_set_user_data(panel, value_label);

    lv_obj_t *handle = lv_obj_create(panel);
    lv_obj_clear_flag(handle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(handle, 40, 5);
    lv_obj_set_style_radius(handle, 3, 0);
    lv_obj_set_style_bg_color(handle, lv_color_hex(0xB8C4D0), 0);
    lv_obj_set_style_border_width(handle, 0, 0);
    lv_obj_align(handle, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *title_row = lv_obj_create(panel);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(title_row, lv_pct(90));
    lv_obj_set_height(title_row, 24);
    lv_obj_set_style_bg_opa(title_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_set_flex_flow(title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align_to(title_row, panel, LV_ALIGN_TOP_MID, 0, 33);

    lv_obj_t *title = lv_label_create(title_row);
    lv_label_set_text(title, ui_str(UI_STR_SATURATION));
    lv_obj_set_style_text_font(title, ui_font_body(), 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x2D3748), 0);

    lv_obj_t *val_lab = lv_label_create(title_row);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", s_select_actions_ctx.state.product.light.saturation);
    lv_label_set_text(val_lab, buf);
    lv_obj_set_style_text_font(val_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(val_lab, lv_color_hex(0x718096), 0);

    const int32_t sat_knob_half = SELECT_ACTIONS_LIGHT_SLIDER_KNOB_D / 2;
    const int32_t sat_slider_row_h = SELECT_ACTIONS_LIGHT_SLIDER_KNOB_D + 8;
    int hue_for_sat = s_select_actions_ctx.state.set.hue ? s_select_actions_ctx.state.product.light.hue : 0;
    select_actions_saturation_grad_update_for_hue(hue_for_sat);

    lv_obj_t *slider_row = lv_obj_create(panel);
    lv_obj_clear_flag(slider_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(slider_row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_width(slider_row, lv_pct(90));
    lv_obj_set_height(slider_row, sat_slider_row_h);
    lv_obj_set_style_bg_opa(slider_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(slider_row, 0, 0);
    lv_obj_align_to(slider_row, title_row, LV_ALIGN_OUT_BOTTOM_MID, 0, 16);

    lv_obj_t *sat_bar = lv_obj_create(slider_row);
    lv_obj_remove_style_all(sat_bar);
    lv_obj_clear_flag(sat_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(sat_bar, lv_pct(100));
    lv_obj_set_height(sat_bar, SELECT_ACTIONS_LIGHT_SLIDER_TRACK_H);
    lv_obj_set_style_pad_left(sat_bar, sat_knob_half, 0);
    lv_obj_set_style_pad_right(sat_bar, sat_knob_half, 0);
    lv_obj_set_style_bg_opa(sat_bar, LV_OPA_0, 0);
    lv_obj_set_style_border_width(sat_bar, 0, 0);
    lv_obj_set_style_radius(sat_bar, SELECT_ACTIONS_LIGHT_SLIDER_TRACK_H / 2, 0);
    lv_obj_set_style_clip_corner(sat_bar, true, 0);
    lv_obj_align(sat_bar, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_grad(sat_bar, &s_sa_saturation_grad, 0);
    lv_obj_set_style_bg_opa(sat_bar, LV_OPA_COVER, 0);

    lv_obj_t *slider = lv_slider_create(slider_row);
    lv_obj_set_width(slider, lv_pct(100));
    lv_obj_set_height(slider, SELECT_ACTIONS_LIGHT_SLIDER_KNOB_D);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(slider, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, s_select_actions_ctx.state.product.light.saturation, LV_ANIM_OFF);
    select_actions_style_light_hue_slider_knob_only(slider);
    lv_obj_add_event_cb(slider, saturation_slider_changed_cb, LV_EVENT_VALUE_CHANGED, val_lab);
    lv_obj_move_foreground(slider);

    lv_obj_t *btn_row = lv_obj_create(panel);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(btn_row, lv_pct(90));
    lv_obj_set_height(btn_row, 48);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_left(btn_row, 8, 0);
    lv_obj_set_style_pad_right(btn_row, 8, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn_row, 12, 0);
    lv_obj_align_to(btn_row, slider_row, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);

    lv_obj_t *left_btn = lv_btn_create(btn_row);
    lv_obj_clear_flag(left_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_height(left_btn, 48);
    lv_obj_set_flex_grow(left_btn, 1);
    lv_obj_set_style_radius(left_btn, 12, 0);
    lv_obj_set_style_border_width(left_btn, 0, 0);
    if (has_value) {
        lv_obj_set_style_bg_color(left_btn, lv_color_hex(0xDC2626), 0);
        lv_obj_set_style_bg_color(left_btn, lv_color_hex(0xB91C1C), LV_STATE_PRESSED);
        lv_obj_add_event_cb(left_btn, saturation_delete_cb, LV_EVENT_CLICKED, sheet);
    } else {
        lv_obj_set_style_bg_color(left_btn, lv_color_hex(0xE8ECF0), 0);
        lv_obj_add_event_cb(left_btn, saturation_cancel_cb, LV_EVENT_CLICKED, sheet);
    }
    lv_obj_t *left_lab = lv_label_create(left_btn);
    lv_label_set_text(left_lab, has_value ? ui_str(UI_STR_DELETE) : ui_str(UI_STR_CANCEL));
    lv_obj_set_style_text_font(left_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(left_lab, lv_color_hex(has_value ? 0xFFFFFF : 0x4A5568), 0);
    lv_obj_center(left_lab);

    lv_obj_t *save_btn = lv_btn_create(btn_row);
    lv_obj_clear_flag(save_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_height(save_btn, 48);
    lv_obj_set_flex_grow(save_btn, 1);
    lv_obj_set_style_radius(save_btn, 12, 0);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x2E6BE6), 0);
    lv_obj_set_style_border_width(save_btn, 0, 0);
    lv_obj_add_event_cb(save_btn, saturation_save_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *save_lab = lv_label_create(save_btn);
    lv_label_set_text(save_lab, ui_str(UI_STR_SAVE));
    lv_obj_set_style_text_font(save_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(save_lab, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(save_lab);

    lv_obj_move_foreground(sheet);
}

#define SELECT_ACTIONS_FAN_SPEED_MIN  0
#define SELECT_ACTIONS_FAN_SPEED_MAX  5

static void show_speed_sheet(lv_obj_t *value_label)
{
    const char *cur = value_label ? lv_label_get_text(value_label) : NULL;
    bool has_value = (cur && cur[0] != '\0');
    if (has_value) {
        int v = atoi(cur);
        if (v >= SELECT_ACTIONS_FAN_SPEED_MIN && v <= SELECT_ACTIONS_FAN_SPEED_MAX) {
            s_select_actions_ctx.state.product.fan.speed = v;
        }
    }

    lv_obj_t *scr = ui_Screen_SelectActions;
    lv_obj_t *sheet = lv_obj_create(scr);
    lv_obj_clear_flag(sheet, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(sheet, lv_pct(100), lv_pct(100));
    lv_obj_align(sheet, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(sheet, LV_OPA_0, 0);
    lv_obj_set_style_border_width(sheet, 0, 0);

    lv_obj_t *panel = lv_obj_create(sheet);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(panel, lv_pct(100));
    lv_obj_set_style_min_height(panel, 220, 0);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 20, 0);
    lv_obj_set_style_pad_top(panel, 12, 0);
    lv_obj_set_style_pad_bottom(panel, 20, 0);
    lv_obj_set_user_data(panel, value_label);

    lv_obj_t *handle = lv_obj_create(panel);
    lv_obj_clear_flag(handle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(handle, 40, 5);
    lv_obj_set_style_radius(handle, 3, 0);
    lv_obj_set_style_bg_color(handle, lv_color_hex(0xB8C4D0), 0);
    lv_obj_set_style_border_width(handle, 0, 0);
    lv_obj_align(handle, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *title_row = lv_obj_create(panel);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(title_row, lv_pct(90));
    lv_obj_set_height(title_row, 24);
    lv_obj_set_style_bg_opa(title_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_set_flex_flow(title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align_to(title_row, panel, LV_ALIGN_TOP_MID, 0, 33);

    lv_obj_t *title = lv_label_create(title_row);
    lv_label_set_text(title, ui_str(UI_STR_SPEED));
    lv_obj_set_style_text_font(title, ui_font_body(), 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x2D3748), 0);

    lv_obj_t *val_lab = lv_label_create(title_row);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", s_select_actions_ctx.state.product.fan.speed);
    lv_label_set_text(val_lab, buf);
    lv_obj_set_style_text_font(val_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(val_lab, lv_color_hex(0x718096), 0);

    lv_obj_t *slider_row = lv_obj_create(panel);
    lv_obj_clear_flag(slider_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(slider_row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_width(slider_row, lv_pct(90));
    lv_obj_set_height(slider_row, 52);
    lv_obj_set_style_bg_opa(slider_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(slider_row, 0, 0);
    lv_obj_align_to(slider_row, title_row, LV_ALIGN_OUT_BOTTOM_MID, 0, 16);

    lv_obj_t *slider = lv_slider_create(slider_row);
    lv_obj_set_width(slider, lv_pct(100));
    lv_obj_set_height(slider, 26);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(slider, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_slider_set_range(slider, SELECT_ACTIONS_FAN_SPEED_MIN, SELECT_ACTIONS_FAN_SPEED_MAX);
    lv_slider_set_value(slider, s_select_actions_ctx.state.product.fan.speed, LV_ANIM_OFF);
    select_actions_style_brightness_slider(slider);
    lv_obj_add_event_cb(slider, speed_slider_changed_cb, LV_EVENT_VALUE_CHANGED, val_lab);

    lv_obj_t *btn_row = lv_obj_create(panel);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(btn_row, lv_pct(90));
    lv_obj_set_height(btn_row, 48);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_left(btn_row, 8, 0);
    lv_obj_set_style_pad_right(btn_row, 8, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn_row, 12, 0);
    lv_obj_align_to(btn_row, slider_row, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);

    lv_obj_t *left_btn = lv_btn_create(btn_row);
    lv_obj_clear_flag(left_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_height(left_btn, 48);
    lv_obj_set_flex_grow(left_btn, 1);
    lv_obj_set_style_radius(left_btn, 12, 0);
    lv_obj_set_style_border_width(left_btn, 0, 0);
    if (has_value) {
        lv_obj_set_style_bg_color(left_btn, lv_color_hex(0xDC2626), 0);
        lv_obj_set_style_bg_color(left_btn, lv_color_hex(0xB91C1C), LV_STATE_PRESSED);
        lv_obj_add_event_cb(left_btn, speed_delete_cb, LV_EVENT_CLICKED, sheet);
    } else {
        lv_obj_set_style_bg_color(left_btn, lv_color_hex(0xE8ECF0), 0);
        lv_obj_add_event_cb(left_btn, speed_cancel_cb, LV_EVENT_CLICKED, sheet);
    }
    lv_obj_t *left_lab = lv_label_create(left_btn);
    lv_label_set_text(left_lab, has_value ? ui_str(UI_STR_DELETE) : ui_str(UI_STR_CANCEL));
    lv_obj_set_style_text_font(left_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(left_lab, lv_color_hex(has_value ? 0xFFFFFF : 0x4A5568), 0);
    lv_obj_center(left_lab);

    lv_obj_t *save_btn = lv_btn_create(btn_row);
    lv_obj_clear_flag(save_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_height(save_btn, 48);
    lv_obj_set_flex_grow(save_btn, 1);
    lv_obj_set_style_radius(save_btn, 12, 0);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x2E6BE6), 0);
    lv_obj_set_style_border_width(save_btn, 0, 0);
    lv_obj_add_event_cb(save_btn, speed_save_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *save_lab = lv_label_create(save_btn);
    lv_label_set_text(save_lab, ui_str(UI_STR_SAVE));
    lv_obj_set_style_text_font(save_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(save_lab, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(save_lab);

    lv_obj_move_foreground(sheet);
}

static void select_actions_row_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    row_ctx_t *ctx = (row_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || !ctx->value_label) {
        return;
    }
    if (strcmp(ctx->action, RM_STANDARD_DEVICE_POWER) == 0) {
        show_power_sheet(ctx->value_label);
    } else if (strcmp(ctx->action, RM_STANDARD_LIGHT_DEVICE_BRIGHTNESS) == 0) {
        show_brightness_sheet(ctx->value_label);
    } else if (strcmp(ctx->action, RM_STANDARD_LIGHT_DEVICE_HUE) == 0) {
        show_hue_sheet(ctx->value_label);
    } else if (strcmp(ctx->action, RM_STANDARD_LIGHT_DEVICE_SATURATION) == 0) {
        show_saturation_sheet(ctx->value_label);
    } else if (strcmp(ctx->action, RM_STANDARD_FAN_DEVICE_SPEED) == 0) {
        show_speed_sheet(ctx->value_label);
    }
}

static lv_obj_t *create_header(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(bar, lv_pct(100));
    lv_obj_set_height(bar, 56);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xF5F8FF), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_left(bar, 8, 0);
    lv_obj_set_style_pad_right(bar, 8, 0);

    lv_obj_t *back = lv_btn_create(bar);
    lv_obj_clear_flag(back, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(back, 40, 40);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_0, 0);
    lv_obj_set_style_border_width(back, 0, 0);
    lv_obj_add_event_cb(back, select_actions_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *arrow = lv_label_create(back);
    lv_label_set_text(arrow, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(arrow, ui_font_montserrat_18(), 0);
    lv_obj_set_style_text_color(arrow, lv_color_hex(0x2E6BE6), 0);
    lv_obj_center(arrow);

    lv_obj_t *title = lv_label_create(bar);
    s_select_actions_title_label = title;
    lv_label_set_text(title, ui_str(UI_STR_SELECT_ACTIONS));
    lv_obj_set_style_text_font(title, ui_font_title(), 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x1A1A1A), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    return bar;
}

void ui_Screen_SelectActions_show(rm_device_item_handle_t device_handle)
{
    s_select_actions_ctx.handle = device_handle;
    /* Reset to defaults for each device so previous device's Power/Brightness/etc. are not reused */
    select_actions_reset_state();

    if (ui_Screen_SelectActions == NULL) {
        return;
    }

    lv_obj_t *list = s_actions_list;
    if (list == NULL) {
        lv_scr_load(ui_Screen_SelectActions);
        return;
    }

    lv_obj_clean(list);

    const char **options = NULL;
    int n = 0;
    if (!select_actions_build_options(device_handle, &options, &n)) {
        lv_scr_load(ui_Screen_SelectActions);
        return;
    }
    for (int i = 0; i < n; i++) {
        row_ctx_t *ctx = (row_ctx_t *)UI_LV_MALLOC(sizeof(row_ctx_t));
        if (!ctx) {
            continue;
        }
        ctx->action = options[i];

        lv_obj_t *row = lv_btn_create(list);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 52);
        lv_obj_set_style_radius(row, 12, 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(0xF0F4F8), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_shadow_width(row, 0, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(0xE0E8F0), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_pad_left(row, 16, 0);
        lv_obj_add_event_cb(row, select_actions_row_cb, LV_EVENT_CLICKED, ctx);
        lv_obj_add_event_cb(row, select_actions_row_delete_cb, LV_EVENT_DELETE, ctx);
        lv_obj_set_user_data(row, ctx);

        lv_obj_t *label = lv_label_create(row);
        lv_label_set_text(label, select_actions_action_display_name(options[i]));
        lv_obj_set_style_text_font(label, ui_font_body(), 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0x1A1A1A), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *value_label = lv_label_create(row);
        lv_label_set_text(value_label, "");
        lv_obj_set_style_text_font(value_label, ui_font_body(), 0);
        lv_obj_set_style_text_color(value_label, lv_color_hex(0x6B7A99), 0);
        lv_obj_align(value_label, LV_ALIGN_RIGHT_MID, -16, 0);
        ctx->value_label = value_label;
    }
    if (options) {
        UI_LV_FREE(options);
    }

    lv_scr_load(ui_Screen_SelectActions);
}

void ui_Screen_SelectActions_show_for_edit(rm_device_item_handle_t device_handle, rm_list_item_t *schedule_item, int action_index)
{
    if (!device_handle || !schedule_item || action_index < 0 || !ui_Screen_SelectActions || !s_actions_list) {
        return;
    }
    rm_node_action_item_t *node_action_item = rm_list_manager_get_node_action_item_by_index(schedule_item, action_index);
    if (!node_action_item) {
        return;
    }

    s_select_actions_ctx.handle = device_handle;
    s_select_actions_ctx.edit_item = schedule_item;
    s_select_actions_ctx.edit_action_index = action_index;

    /* Pre-fill state from existing action JSON (do not reset to defaults) */
    select_actions_parse_action_json_to_state(device_handle, node_action_item->action);

    lv_obj_clean(s_actions_list);
    const char **options = NULL;
    int n = 0;
    if (!select_actions_build_options(device_handle, &options, &n)) {
        s_select_actions_ctx.edit_item = NULL;
        s_select_actions_ctx.edit_action_index = -1;
        lv_scr_load(ui_Screen_SelectActions);
        return;
    }
    for (int i = 0; i < n; i++) {
        row_ctx_t *ctx = (row_ctx_t *)UI_LV_MALLOC(sizeof(row_ctx_t));
        if (!ctx) {
            continue;
        }
        ctx->action = options[i];

        lv_obj_t *row = lv_btn_create(s_actions_list);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 52);
        lv_obj_set_style_radius(row, 12, 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(0xF0F4F8), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_shadow_width(row, 0, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(0xE0E8F0), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_pad_left(row, 16, 0);
        lv_obj_add_event_cb(row, select_actions_row_cb, LV_EVENT_CLICKED, ctx);
        lv_obj_add_event_cb(row, select_actions_row_delete_cb, LV_EVENT_DELETE, ctx);
        lv_obj_set_user_data(row, ctx);

        lv_obj_t *label = lv_label_create(row);
        lv_label_set_text(label, select_actions_action_display_name(options[i]));
        lv_obj_set_style_text_font(label, ui_font_body(), 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0x1A1A1A), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *value_label = lv_label_create(row);
        lv_label_set_text(value_label, "");
        lv_obj_set_style_text_font(value_label, ui_font_body(), 0);
        lv_obj_set_style_text_color(value_label, lv_color_hex(0x6B7A99), 0);
        lv_obj_align(value_label, LV_ALIGN_RIGHT_MID, -16, 0);
        ctx->value_label = value_label;
    }
    if (options) {
        UI_LV_FREE(options);
    }

    select_actions_set_initial_value_labels();
    lv_scr_load(ui_Screen_SelectActions);
}

void ui_Screen_SelectActions_screen_init(void)
{
    ui_Screen_SelectActions = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_Screen_SelectActions, lv_color_hex(0xF0F4F8), 0);
    lv_obj_clear_flag(ui_Screen_SelectActions, LV_OBJ_FLAG_SCROLLABLE);

    create_header(ui_Screen_SelectActions);

    lv_coord_t scr_h = lv_disp_get_ver_res(lv_disp_get_default());
    if (scr_h <= 0) {
        scr_h = 480;
    }
    const int32_t top_bar = 56;

    lv_obj_t *content = lv_obj_create(ui_Screen_SelectActions);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(content, lv_pct(100));
    lv_obj_set_height(content, LV_MAX(40, scr_h - top_bar));
    lv_obj_align_to(content, ui_Screen_SelectActions, LV_ALIGN_TOP_LEFT, 0, top_bar);
    lv_obj_set_style_bg_opa(content, LV_OPA_0, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(content, 16, 0);
    lv_obj_set_style_pad_row(content, 12, 0);

    lv_obj_t *scroll_mid = lv_obj_create(content);
    lv_obj_add_flag(scroll_mid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(scroll_mid, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll_mid, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_width(scroll_mid, lv_pct(100));
    lv_obj_set_flex_grow(scroll_mid, 1);
    lv_obj_set_style_bg_opa(scroll_mid, LV_OPA_0, 0);
    lv_obj_set_style_border_width(scroll_mid, 0, 0);
    lv_obj_set_flex_flow(scroll_mid, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scroll_mid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(scroll_mid, 12, 0);

    lv_obj_t *list = lv_obj_create(scroll_mid);
    lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(list, 12, 0);
    lv_obj_set_width(list, lv_pct(100));
    lv_obj_set_height(list, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(list, LV_OPA_0, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    s_actions_list = list;

    lv_obj_t *done_btn = lv_btn_create(content);
    lv_obj_clear_flag(done_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(done_btn, lv_pct(100));
    lv_obj_set_height(done_btn, 48);
    lv_obj_set_style_radius(done_btn, 12, 0);
    lv_obj_set_style_bg_color(done_btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_color(done_btn, lv_color_hex(0xE8ECF0), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(done_btn, 0, 0);
    lv_obj_set_style_border_color(done_btn, lv_color_hex(0xE0E8F0), 0);
    lv_obj_add_event_cb(done_btn, select_actions_done_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *done_lab = lv_label_create(done_btn);
    s_select_actions_done_lab = done_lab;
    lv_label_set_text(done_lab, ui_str(UI_STR_DONE));
    lv_obj_set_style_text_font(done_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(done_lab, lv_color_hex(0x4A5568), 0);
    lv_obj_center(done_lab);
}

void ui_Screen_SelectActions_screen_destroy(void)
{
    if (ui_Screen_SelectActions) {
        lv_obj_del(ui_Screen_SelectActions);
    }
    ui_Screen_SelectActions = NULL;
    s_select_actions_title_label = NULL;
    s_select_actions_done_lab = NULL;
    s_select_actions_ctx.handle = NULL;
    s_select_actions_ctx.edit_item = NULL;
    s_select_actions_ctx.edit_action_index = -1;
    select_actions_reset_state();
}

void ui_Screen_SelectActions_apply_language(void)
{
    if (s_select_actions_title_label) {
        lv_label_set_text(s_select_actions_title_label, ui_str(UI_STR_SELECT_ACTIONS));
        lv_obj_set_style_text_font(s_select_actions_title_label, ui_font_title(), 0);
    }
    if (s_select_actions_done_lab) {
        lv_label_set_text(s_select_actions_done_lab, ui_str(UI_STR_DONE));
        lv_obj_set_style_text_font(s_select_actions_done_lab, ui_font_body(), 0);
    }
    if (s_select_actions_ctx.handle && s_actions_list && ui_Screen_SelectActions &&
            lv_scr_act() == ui_Screen_SelectActions) {
        if (s_select_actions_ctx.edit_item != NULL && s_select_actions_ctx.edit_action_index >= 0) {
            ui_Screen_SelectActions_show_for_edit(s_select_actions_ctx.handle, s_select_actions_ctx.edit_item,
                                                  s_select_actions_ctx.edit_action_index);
        } else {
            ui_Screen_SelectActions_show(s_select_actions_ctx.handle);
        }
    }
}
